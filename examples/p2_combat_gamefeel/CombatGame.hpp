#pragma once

#include <trace2d/application/Application.hpp>
#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>
#include <trace2d/physics/PhysicsComponents2D.hpp>
#include <trace2d/physics/PhysicsWorld2D.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace trace2d::examples
{
class CombatGame final : public application::Game
{
public:
    static constexpr float CanvasWidth = 640.0F;
    static constexpr float CanvasHeight = 360.0F;
    static constexpr float WorldHalfWidth = 8.0F;
    static constexpr float WorldHalfHeight = 4.5F;
    static constexpr float PlayerStartX = -5.0F;
    static constexpr float PlayerStartY = 0.0F;
    static constexpr float EnemyStartX = 2.8F;
    static constexpr float EnemyStartY = 0.0F;
    static constexpr float PlayerRadius = 0.45F;
    static constexpr float EnemyRadius = 0.55F;
    static constexpr float MoveSpeed = 4.0F;
    static constexpr float AttackReach = 1.15F;
    static constexpr float AttackHalfWidth = 0.80F;
    static constexpr float AttackHalfHeight = 0.70F;
    static constexpr float KnockbackImpulse = 4.0F;
    static constexpr std::uint32_t MaximumEnemyHealth = 3U;
    static constexpr std::uint32_t AttackCooldownFrames = 18U;
    static constexpr std::uint32_t HitFlashFrames = 6U;

    CombatGame(
        physics::PhysicsComponentTypes2D physicsTypes,
        audio::AudioComponentTypes2D audioTypes,
        std::filesystem::path projectRoot);

    void OnStart(application::GameContext& context) override;
    void OnFixedUpdate(application::GameContext& context, const application::FixedUpdate& update) override;
    void OnStop(application::GameContext& context) override;

    [[nodiscard]] std::uint32_t EnemyHealth() const noexcept;
    [[nodiscard]] std::uint32_t AcceptedHitCount() const noexcept;
    [[nodiscard]] std::uint32_t AcceptedSwingCount() const noexcept;
    [[nodiscard]] std::uint32_t CooldownFramesRemaining() const noexcept;
    [[nodiscard]] std::uint32_t HitFlashFramesRemaining() const noexcept;
    [[nodiscard]] scene::Vector2 Facing() const noexcept;
    [[nodiscard]] bool LastSwingHitEnemy() const noexcept;
    [[nodiscard]] std::uint64_t SemanticStartedEventCount() const noexcept;

    [[nodiscard]] bool TryPlayerBodyState(physics::PhysicsBodyState2D& out) const noexcept;
    [[nodiscard]] bool TryEnemyBodyState(physics::PhysicsBodyState2D& out) const noexcept;
    [[nodiscard]] physics::PhysicsMetrics2D PhysicsMetrics() const noexcept;
    [[nodiscard]] audio::AudioMetrics2D AudioMetrics() const noexcept;
    [[nodiscard]] assets::ResourceRegistryStats ResourceStats() const noexcept;

    [[nodiscard]] audio::AudioSystem2D& Audio();
    [[nodiscard]] const audio::AudioSystem2D& Audio() const;
    [[nodiscard]] assets::ResourceRegistry& Resources() noexcept;
    [[nodiscard]] const assets::ResourceRegistry& Resources() const noexcept;

private:
    [[nodiscard]] scene::EntityId CreatePhysicalEntity(
        scene::Scene& scene,
        const char* semanticId,
        scene::Vector2 position,
        physics::RigidBody2D body,
        physics::Collider2D collider);
    void ResetRound(application::GameContext& context);
    void TryAttack(application::GameContext& context);
    void UpdateEnemyHealthUi(application::GameContext& context);

    physics::PhysicsComponentTypes2D physicsTypes_{};
    audio::AudioComponentTypes2D audioTypes_{};
    assets::ResourceRegistry resources_;
    std::unique_ptr<physics::PhysicsWorld2D> physics_{};
    std::unique_ptr<audio::AudioSystem2D> audio_{};

    scene::EntityId player_{};
    scene::EntityId enemy_{};
    scene::EntityId hitAudioEntity_{};
    audio::AudioVoiceHandle2D lastHitVoice_{};

    input::Axis1DActionId horizontalAction_{};
    input::Axis1DActionId verticalAction_{};
    input::ButtonActionId attackAction_{};
    input::ButtonActionId restartAction_{};

    scene::Vector2 facing_{1.0F, 0.0F};
    std::uint32_t enemyHealth_{MaximumEnemyHealth};
    std::uint32_t acceptedHitCount_{0U};
    std::uint32_t acceptedSwingCount_{0U};
    std::uint32_t cooldownFramesRemaining_{0U};
    std::uint32_t hitFlashFramesRemaining_{0U};
    std::uint64_t semanticStartedEventCount_{0U};
    bool lastSwingHitEnemy_{false};
};
} // namespace trace2d::examples
