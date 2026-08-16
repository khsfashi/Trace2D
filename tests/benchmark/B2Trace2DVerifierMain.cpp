#include "Trace2DB2Candidate.hpp"

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/input/Input.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
using trace2d::agent::ComponentFieldSnapshot;
using trace2d::agent::ComponentSnapshot;
using trace2d::agent::EntitySnapshot;
using trace2d::agent::FieldValueKind;

void Require(const bool condition, const std::string_view message)
{
    if (!condition) throw std::runtime_error{std::string{message}};
}

[[nodiscard]] EntitySnapshot QueryEntity(
    const trace2d::agent::AgentFacade& facade,
    const std::string_view selector)
{
    const auto query = facade.QueryOne(selector);
    Require(query.Succeeded() && query.match.has_value(), "required B2 entity query failed");
    return *query.match;
}

[[nodiscard]] const ComponentSnapshot& RequireComponent(
    const EntitySnapshot& entity,
    const std::string_view type)
{
    for (const auto& component : entity.components)
    {
        if (std::string_view{component.type} == type) return component;
    }
    throw std::runtime_error{"required B2 component is missing"};
}

[[nodiscard]] const ComponentFieldSnapshot& RequireField(
    const ComponentSnapshot& component,
    const std::string_view name)
{
    for (const auto& field : component.fields)
    {
        if (std::string_view{field.name} == name) return field;
    }
    throw std::runtime_error{"required B2 component field is missing"};
}

[[nodiscard]] std::int64_t ReadInt(
    const ComponentSnapshot& component,
    const std::string_view name)
{
    const auto& field = RequireField(component, name);
    Require(field.value.kind == FieldValueKind::SignedInteger, "B2 integer field changed type");
    return field.value.signedIntegerValue;
}

[[nodiscard]] bool ReadBool(
    const ComponentSnapshot& component,
    const std::string_view name)
{
    const auto& field = RequireField(component, name);
    Require(field.value.kind == FieldValueKind::Boolean, "B2 boolean field changed type");
    return field.value.booleanValue;
}

void RequireCombatState(
    const EntitySnapshot& entity,
    const std::int64_t hp,
    const std::int64_t maximumHp,
    const bool dead,
    const std::int64_t deathTransitions)
{
    const auto& combat = RequireComponent(entity, "b2.combatant");
    Require(ReadInt(combat, "hp") == hp, "B2 hp mismatch");
    Require(ReadInt(combat, "maximum_hp") == maximumHp, "B2 maximum hp mismatch");
    Require(ReadBool(combat, "dead") == dead, "B2 dead-state mismatch");
    Require(ReadInt(combat, "death_transitions") == deathTransitions, "B2 death-transition mismatch");
}

void RequireFeedbackState(
    const EntitySnapshot& enemy,
    const std::int64_t hitTriggers,
    const std::int64_t deathTriggers)
{
    const auto& feedback = RequireComponent(enemy, "b2.presentation_hooks");
    Require(ReadInt(feedback, "hit_particle_triggers") == hitTriggers, "B2 hit feedback trigger mismatch");
    Require(ReadInt(feedback, "death_particle_triggers") == deathTriggers, "B2 death feedback trigger mismatch");
}

void RequireHud(
    const trace2d::agent::AgentFacade& facade,
    const std::string_view id,
    const std::uint32_t value,
    const std::uint32_t maximum)
{
    const auto query = facade.QueryOneUi(trace2d::agent::UiSelector{.id = std::string{id}});
    Require(query.Succeeded() && query.match.has_value(), "required B2 HUD element query failed");
    Require(query.match->role == trace2d::agent::UiRole::ProgressBar, "B2 HUD element is not a progress bar");
    Require(query.match->progressValue == value, "B2 HUD progress value mismatch");
    Require(query.match->progressMaximum == maximum, "B2 HUD progress maximum mismatch");
}

void RequireActions(const trace2d::application::Application& application)
{
    constexpr std::array<std::string_view, 5U> required{
        "move_left",
        "move_right",
        "move_up",
        "move_down",
        "attack",
    };
    for (const std::string_view action : required)
    {
        Require(application.Actions().FindButtonAction(action).has_value(), "required B2 semantic action is missing");
    }
}
} // namespace

int main()
{
    try
    {
        using trace2d::input::InputControl;
        using trace2d::input::InputEvent;
        using trace2d::input::InputEventType;

        trace2d::scene::ComponentRegistry registry{};
        auto game = trace2d::benchmark::b2::CreateCandidate(registry);
        Require(game != nullptr, "B2 candidate factory returned no Game");
        registry.Freeze();

        trace2d::application::ApplicationConfig config{};
        config.scene.semanticId = "b2.trace2d.verifier";
        config.scene.name = "B2 Trace2D verifier";
        config.uiWidth = 320U;
        config.uiHeight = 180U;

        trace2d::application::Application application{*game, registry, config};

        // Verifier-owned physical inputs exercise the same normal Input -> ActionMap -> Game path
        // used by an external host. Candidate code never receives direct state-assignment hooks.
        application.ScheduleInput(1U, InputEvent{.control = InputControl::KeyD, .type = InputEventType::Press});
        application.ScheduleInput(9U, InputEvent{.control = InputControl::KeyD, .type = InputEventType::Release});
        application.ScheduleInput(9U, InputEvent{.control = InputControl::Space, .type = InputEventType::Press});
        application.ScheduleInput(10U, InputEvent{.control = InputControl::Space, .type = InputEventType::Release});
        application.ScheduleInput(14U, InputEvent{.control = InputControl::Space, .type = InputEventType::Press});
        application.ScheduleInput(15U, InputEvent{.control = InputControl::Space, .type = InputEventType::Release});
        application.ScheduleInput(16U, InputEvent{.control = InputControl::Space, .type = InputEventType::Press});

        application.Start();
        RequireActions(application);

        trace2d::agent::AgentFacade facade{
            &application.Runtime(),
            &application.Scene(),
            &application.Ui()};

        // Frozen initial state.
        auto player = QueryEntity(facade, "#player");
        auto enemy = QueryEntity(facade, "#enemy");
        Require(player.transform.position.x == 0.0F && player.transform.position.y == 0.0F,
            "B2 player initial position mismatch");
        Require(enemy.transform.position.x == 64.0F && enemy.transform.position.y == 0.0F,
            "B2 enemy initial position mismatch");
        RequireCombatState(player, 3, 3, false, 0);
        RequireCombatState(enemy, 2, 2, false, 0);
        RequireFeedbackState(enemy, 0, 0);
        RequireHud(facade, "hud.player_hp", 3U, 3U);
        RequireHud(facade, "hud.enemy_hp", 2U, 2U);

        // Eight ordinary fixed steps while move_right is held must move exactly 32 units.
        application.StepFrames(8U);
        player = QueryEntity(facade, "#player");
        enemy = QueryEntity(facade, "#enemy");
        Require(player.transform.position.x == 32.0F && player.transform.position.y == 0.0F,
            "B2 eight-step movement mismatch");
        RequireCombatState(player, 3, 3, false, 0);
        RequireCombatState(enemy, 2, 2, false, 0);

        // First normal attack at frame 9 deals exactly one damage and exercises hit feedback.
        application.StepFrames(1U);
        enemy = QueryEntity(facade, "#enemy");
        RequireCombatState(enemy, 1, 2, false, 0);
        RequireFeedbackState(enemy, 1, 0);
        RequireHud(facade, "hud.enemy_hp", 1U, 2U);

        // Frame 14 is still inside a six-fixed-step cooldown from the frame-9 attack. A common
        // off-by-one/5-step implementation therefore becomes a meaningful known-bad fixture.
        application.StepFrames(5U);
        enemy = QueryEntity(facade, "#enemy");
        RequireCombatState(enemy, 1, 2, false, 0);
        RequireFeedbackState(enemy, 1, 0);

        // After the full cooldown, the frame-16 attack is lethal exactly once.
        application.StepFrames(2U);
        player = QueryEntity(facade, "#player");
        enemy = QueryEntity(facade, "#enemy");
        RequireCombatState(player, 3, 3, false, 0);
        RequireCombatState(enemy, 0, 2, true, 1);
        RequireFeedbackState(enemy, 2, 1);
        RequireHud(facade, "hud.player_hp", 3U, 3U);
        RequireHud(facade, "hud.enemy_hp", 0U, 2U);
        Require(application.Snapshot().frame == 16U, "B2 verifier did not replay exactly sixteen fixed steps");

        application.Stop();
        std::cout << "B2 Trace2D verifier accepted candidate: deterministic gameplay/HUD/feedback contract passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "B2 Trace2D verifier rejected candidate: " << error.what() << '\n';
        return 1;
    }
}
