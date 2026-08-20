#include "CombatGame.hpp"

#include <trace2d/application/Application.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/physics/PhysicsComponents2D.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>

#ifndef TRACE2D_P2_SOURCE_DIR
#define TRACE2D_P2_SOURCE_DIR "."
#endif

namespace
{
using trace2d::examples::CombatGame;

struct CombatResult final
{
    trace2d::physics::PhysicsBodyState2D player{};
    trace2d::physics::PhysicsBodyState2D enemy{};
    std::uint32_t health{0U};
    std::uint32_t hits{0U};
    std::uint32_t swings{0U};
    std::uint64_t startedEvents{0U};
    trace2d::physics::PhysicsMetrics2D physics{};
    trace2d::audio::AudioMetrics2D audio{};
    trace2d::assets::ResourceRegistryStats resources{};
};

int Fail(const int code, const std::string_view message)
{
    std::cerr << "Trace2D P2 combat proof failure [" << code << "]: " << message << '\n';
    return code;
}

[[nodiscard]] bool Near(const float left, const float right, const float tolerance = 0.02F) noexcept
{
    return std::abs(left - right) <= tolerance;
}

trace2d::application::ApplicationConfig MakeConfig()
{
    trace2d::application::ApplicationConfig config{};
    config.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
    config.runtime.seed = 329U;
    config.scene.semanticId = "p2.combat-gamefeel";
    config.scene.name = "P2 Combat Game Feel";
    config.uiWidth = static_cast<std::uint32_t>(CombatGame::CanvasWidth);
    config.uiHeight = static_cast<std::uint32_t>(CombatGame::CanvasHeight);
    return config;
}

struct RegistryTypes final
{
    trace2d::scene::ComponentRegistry registry{};
    trace2d::physics::PhysicsComponentTypes2D physics{};
    trace2d::audio::AudioComponentTypes2D audio{};

    RegistryTypes()
        : physics{trace2d::physics::RegisterPhysics2DComponents(registry)}
        , audio{trace2d::audio::RegisterAudio2DComponents(registry)}
    {
        registry.Freeze();
    }
};

[[nodiscard]] CombatResult RunCombatScenario()
{
    RegistryTypes types{};
    CombatGame game{types.physics, types.audio, TRACE2D_P2_SOURCE_DIR};
    trace2d::application::Application application{game, types.registry, MakeConfig()};

    application.ScheduleInput(1U, {
        .control = trace2d::input::InputControl::KeyD,
        .type = trace2d::input::InputEventType::Press,
    });
    application.ScheduleInput(82U, {
        .control = trace2d::input::InputControl::KeyD,
        .type = trace2d::input::InputEventType::Release,
    });
    application.ScheduleInput(83U, {
        .control = trace2d::input::InputControl::Space,
        .type = trace2d::input::InputEventType::Press,
    });
    application.ScheduleInput(84U, {
        .control = trace2d::input::InputControl::Space,
        .type = trace2d::input::InputEventType::Release,
    });
    // Deliberately retry during the authored cooldown. This input must not become a second swing.
    application.ScheduleInput(85U, {
        .control = trace2d::input::InputControl::Space,
        .type = trace2d::input::InputEventType::Press,
    });
    application.ScheduleInput(86U, {
        .control = trace2d::input::InputControl::Space,
        .type = trace2d::input::InputEventType::Release,
    });

    application.Start();
    application.StepFrames(90U);

    CombatResult result{};
    if (!game.TryPlayerBodyState(result.player) || !game.TryEnemyBodyState(result.enemy))
        throw std::runtime_error{"P2 combat scenario could not inspect public body state."};
    result.health = game.EnemyHealth();
    result.hits = game.AcceptedHitCount();
    result.swings = game.AcceptedSwingCount();
    result.startedEvents = game.SemanticStartedEventCount();
    result.physics = game.PhysicsMetrics();
    result.audio = game.AudioMetrics();
    result.resources = game.ResourceStats();
    application.Stop();
    return result;
}
} // namespace

int main()
{
    try
    {
        // Collision proof: velocity commands attempt to drive the player through the left wall for
        // long enough that a transform-only implementation would be far outside the arena.
        {
            RegistryTypes types{};
            CombatGame game{types.physics, types.audio, TRACE2D_P2_SOURCE_DIR};
            trace2d::application::Application application{game, types.registry, MakeConfig()};
            application.ScheduleInput(1U, {
                .control = trace2d::input::InputControl::KeyA,
                .type = trace2d::input::InputEventType::Press,
            });
            application.ScheduleInput(121U, {
                .control = trace2d::input::InputControl::KeyA,
                .type = trace2d::input::InputEventType::Release,
            });
            application.Start();
            application.StepFrames(125U);

            trace2d::physics::PhysicsBodyState2D player{};
            if (!game.TryPlayerBodyState(player)) return Fail(1, "collision scenario did not expose player body state");
            if (player.position.x <= -7.25F)
                return Fail(2, "player crossed the authored left collision boundary");
            if (player.position.x >= CombatGame::PlayerStartX - 1.0F)
                return Fail(3, "collision scenario did not move far enough to exercise the wall");

            const trace2d::physics::PhysicsMetrics2D metrics = game.PhysicsMetrics();
            if (metrics.attachedBodyCount != 7U || metrics.fixedStepCount != 125U)
                return Fail(4, "collision scenario did not retain the expected bounded Physics2D world");
            if (metrics.bodyCommandCount < 125U || metrics.bodyCommandFailureCount != 0U)
                return Fail(5, "steady player velocity commands were not accepted cleanly");
            application.Stop();
        }

        const CombatResult first = RunCombatScenario();
        if (first.health != CombatGame::MaximumEnemyHealth - 1U || first.hits != 1U)
            return Fail(6, "one authored attack did not produce exactly one accepted hit");
        if (first.swings != 1U)
            return Fail(7, "cooldown accepted a second swing before the authored interval elapsed");
        if (first.startedEvents != 1U || first.audio.commandFailureCount != 0U)
            return Fail(8, "accepted hit did not publish exactly one semantic Started SFX event");
        if (first.enemy.linearVelocity.x <= 0.0F && first.enemy.position.x <= CombatGame::EnemyStartX)
            return Fail(9, "accepted hit did not produce positive-direction knockback evidence");
        if (first.physics.overlapQueryCount != 1U || first.physics.overlapCapacityFailureCount != 0U)
            return Fail(10, "attack did not use one successful bounded public overlap query");
        if (first.physics.bodyCommandFailureCount != 0U || first.physics.retainedOverlapHitCapacity < 4U)
            return Fail(11, "combat scenario did not preserve bounded Physics2D command/query storage");
        if (first.audio.retainedVoiceCapacity < 4U || first.audio.retainedEventCapacity < 32U)
            return Fail(12, "combat scenario did not preserve bounded semantic audio storage");
        if (first.resources.readyResources < 1U || first.resources.filesystemQueries != 0U)
            return Fail(13, "combat hot path unexpectedly required ResourceRegistry filesystem discovery");

        // Re-run from a new registry/world with the same authored input. Backend float state is
        // compared with a tolerance, while authoritative counters/state must match exactly.
        const CombatResult second = RunCombatScenario();
        if (second.health != first.health || second.hits != first.hits || second.swings != first.swings ||
            second.startedEvents != first.startedEvents)
            return Fail(14, "same authored replay changed authoritative combat/audio results");
        if (!Near(second.player.position.x, first.player.position.x) ||
            !Near(second.player.position.y, first.player.position.y) ||
            !Near(second.enemy.position.x, first.enemy.position.x) ||
            !Near(second.enemy.position.y, first.enemy.position.y))
            return Fail(15, "same authored replay exceeded the documented floating-point tolerance");
        if (second.physics.fixedStepCount != first.physics.fixedStepCount ||
            second.physics.overlapQueryCount != first.physics.overlapQueryCount ||
            second.audio.commandCount != first.audio.commandCount)
            return Fail(16, "same authored replay changed structural work counters");

        std::cout
            << "P2 headless PASS: health=" << first.health
            << ", hits=" << first.hits
            << ", swings=" << first.swings
            << ", started_sfx=" << first.startedEvents
            << ", physics_bodies=" << first.physics.attachedBodyCount
            << ", overlap_capacity=" << first.physics.retainedOverlapHitCapacity
            << ", audio_voice_capacity=" << first.audio.retainedVoiceCapacity
            << ", audio_event_capacity=" << first.audio.retainedEventCapacity
            << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Trace2D P2 combat proof exception: " << error.what() << '\n';
        return 30;
    }
}
