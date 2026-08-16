#pragma once

#include "Trace2DB2Candidate.hpp"

#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/ui/Ui.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace trace2d::benchmark::b2::qualification
{
struct CombatState final
{
    std::int64_t hp{0};
    std::int64_t maximumHp{0};
    bool dead{false};
    std::int64_t deathTransitions{0};
};

struct PresentationHookState final
{
    std::int64_t hitParticleTriggers{0};
    std::int64_t deathParticleTriggers{0};
};

struct ComponentTypes final
{
    scene::ComponentTypeHandle<CombatState> combat{};
    scene::ComponentTypeHandle<PresentationHookState> presentationHooks{};
};

[[nodiscard]] inline scene::SemanticValue IntValue(const std::int64_t value)
{
    scene::SemanticValue result{};
    result.kind = scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

[[nodiscard]] inline scene::SemanticValue BoolValue(const bool value)
{
    scene::SemanticValue result{};
    result.kind = scene::SemanticValueKind::Boolean;
    result.booleanValue = value;
    return result;
}

[[nodiscard]] inline ComponentTypes RegisterQualificationComponents(scene::ComponentRegistry& registry)
{
    scene::ComponentRegistration<CombatState> combat{};
    combat.typeId = "b2.combatant";
    combat.schemaVersion = 1U;
    combat.componentClass = scene::ComponentClass::RuntimeOnly;
    combat.inspect = [](const CombatState& state)
    {
        return std::vector<scene::ComponentInspectionField>{
            {.name = "hp", .value = IntValue(state.hp)},
            {.name = "maximum_hp", .value = IntValue(state.maximumHp)},
            {.name = "dead", .value = BoolValue(state.dead)},
            {.name = "death_transitions", .value = IntValue(state.deathTransitions)},
        };
    };

    scene::ComponentRegistration<PresentationHookState> hooks{};
    hooks.typeId = "b2.presentation_hooks";
    hooks.schemaVersion = 1U;
    hooks.componentClass = scene::ComponentClass::RuntimeOnly;
    hooks.inspect = [](const PresentationHookState& state)
    {
        return std::vector<scene::ComponentInspectionField>{
            {.name = "hit_particle_triggers", .value = IntValue(state.hitParticleTriggers)},
            {.name = "death_particle_triggers", .value = IntValue(state.deathParticleTriggers)},
        };
    };

    return ComponentTypes{
        .combat = registry.Register<CombatState>(std::move(combat)),
        .presentationHooks = registry.Register<PresentationHookState>(std::move(hooks)),
    };
}

class QualificationGame final : public application::Game
{
public:
    QualificationGame(ComponentTypes types, const std::uint32_t cooldownSteps) noexcept
        : types_{types}
        , cooldownSteps_{cooldownSteps}
    {
    }

    void OnStart(application::GameContext& context) override
    {
        moveLeft_ = context.Actions().AddButtonAction("move_left");
        context.Actions().BindButton(moveLeft_, input::InputControl::KeyA);
        moveRight_ = context.Actions().AddButtonAction("move_right");
        context.Actions().BindButton(moveRight_, input::InputControl::KeyD);
        moveUp_ = context.Actions().AddButtonAction("move_up");
        context.Actions().BindButton(moveUp_, input::InputControl::KeyW);
        moveDown_ = context.Actions().AddButtonAction("move_down");
        context.Actions().BindButton(moveDown_, input::InputControl::KeyS);
        attack_ = context.Actions().AddButtonAction("attack");
        context.Actions().BindButton(attack_, input::InputControl::Space);

        scene::EntityDescriptor player{};
        player.semanticId = "player";
        player.name = "Player";
        player.transform.position = {0.0F, 0.0F};
        player_ = context.Scene().CreateEntity(std::move(player));
        static_cast<void>(context.Scene().AddComponent(
            player_,
            types_.combat,
            CombatState{.hp = 3, .maximumHp = 3}));

        scene::EntityDescriptor enemy{};
        enemy.semanticId = "enemy";
        enemy.name = "Enemy";
        enemy.transform.position = {64.0F, 0.0F};
        enemy_ = context.Scene().CreateEntity(std::move(enemy));
        static_cast<void>(context.Scene().AddComponent(
            enemy_,
            types_.combat,
            CombatState{.hp = 2, .maximumHp = 2}));
        static_cast<void>(context.Scene().AddComponent(
            enemy_,
            types_.presentationHooks,
            PresentationHookState{}));

        context.Ui().ReserveElements(2U);
        ui::UiElement playerHp{};
        playerHp.id = "hud.player_hp";
        playerHp.kind = ui::UiElementKind::Panel;
        playerHp.name = "Player HP";
        playerHp.bounds = ui::UiRect{8U, 8U, 120U, 12U};
        if (context.Ui().AddElement(std::move(playerHp)) != ui::UiActionResult::Success ||
            context.Ui().ConfigureProgress("hud.player_hp", 3U, 3U) != ui::UiProgressResult::Success)
        {
            throw std::logic_error{"could not create player HP HUD"};
        }

        ui::UiElement enemyHp{};
        enemyHp.id = "hud.enemy_hp";
        enemyHp.kind = ui::UiElementKind::Panel;
        enemyHp.name = "Enemy HP / state";
        enemyHp.bounds = ui::UiRect{8U, 28U, 120U, 12U};
        if (context.Ui().AddElement(std::move(enemyHp)) != ui::UiActionResult::Success ||
            context.Ui().ConfigureProgress("hud.enemy_hp", 2U, 2U) != ui::UiProgressResult::Success)
        {
            throw std::logic_error{"could not create enemy HP HUD"};
        }
    }

    void OnFixedUpdate(application::GameContext& context, const application::FixedUpdate&) override
    {
        scene::Entity* const playerEntity = context.Scene().TryGet(player_);
        scene::Entity* const enemyEntity = context.Scene().TryGet(enemy_);
        CombatState* const playerCombat = context.Scene().TryGetComponent(player_, types_.combat);
        CombatState* const enemyCombat = context.Scene().TryGetComponent(enemy_, types_.combat);
        PresentationHookState* const hooks = context.Scene().TryGetComponent(enemy_, types_.presentationHooks);
        if (playerEntity == nullptr || enemyEntity == nullptr || playerCombat == nullptr ||
            enemyCombat == nullptr || hooks == nullptr)
        {
            throw std::logic_error{"qualification game lost required runtime state"};
        }

        auto& position = playerEntity->Transform().position;
        if (context.Actions().Held(moveLeft_)) position.x -= 4.0F;
        if (context.Actions().Held(moveRight_)) position.x += 4.0F;
        if (context.Actions().Held(moveUp_)) position.y -= 4.0F;
        if (context.Actions().Held(moveDown_)) position.y += 4.0F;

        if (cooldownRemaining_ > 0U) --cooldownRemaining_;
        if (context.Actions().Pressed(attack_) && cooldownRemaining_ == 0U && !enemyCombat->dead)
        {
            const float dx = std::abs(enemyEntity->Transform().position.x - position.x);
            const float dy = std::abs(enemyEntity->Transform().position.y - position.y);
            if (dx <= 32.0F && dy <= 32.0F)
            {
                --enemyCombat->hp;
                ++hooks->hitParticleTriggers;
                cooldownRemaining_ = cooldownSteps_;
                if (enemyCombat->hp == 0 && !enemyCombat->dead)
                {
                    enemyCombat->dead = true;
                    ++enemyCombat->deathTransitions;
                    ++hooks->deathParticleTriggers;
                }
            }
        }

        if (context.Ui().SetProgress(
                "hud.player_hp",
                static_cast<std::uint32_t>(playerCombat->hp),
                static_cast<std::uint32_t>(playerCombat->maximumHp)) != ui::UiProgressResult::Success ||
            context.Ui().SetProgress(
                "hud.enemy_hp",
                static_cast<std::uint32_t>(enemyCombat->hp),
                static_cast<std::uint32_t>(enemyCombat->maximumHp)) != ui::UiProgressResult::Success)
        {
            throw std::logic_error{"qualification game could not update HUD"};
        }
    }

private:
    ComponentTypes types_{};
    std::uint32_t cooldownSteps_{0U};
    std::uint32_t cooldownRemaining_{0U};
    scene::EntityId player_{};
    scene::EntityId enemy_{};
    input::ButtonActionId moveLeft_{};
    input::ButtonActionId moveRight_{};
    input::ButtonActionId moveUp_{};
    input::ButtonActionId moveDown_{};
    input::ButtonActionId attack_{};
};

[[nodiscard]] inline std::unique_ptr<application::Game> CreateQualificationCandidate(
    scene::ComponentRegistry& registry,
    const std::uint32_t cooldownSteps)
{
    return std::make_unique<QualificationGame>(RegisterQualificationComponents(registry), cooldownSteps);
}
} // namespace trace2d::benchmark::b2::qualification
