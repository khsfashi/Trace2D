#pragma once

#include <trace2d/application/Application.hpp>
#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/scene/Scene.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

class NightfallSurvivorsGame final : public trace2d::application::Game
{
public:
    enum class State : std::uint8_t
    {
        Idle = 0,
        Running,
        LevelUp,
        Won,
        Lost,
    };

    enum class EnemyKind : std::uint8_t
    {
        Ghoul = 0,
        Brute,
        Wisp,
    };

    enum class EffectKind : std::uint8_t
    {
        Hit = 0,
        Death,
        LevelUp,
        PlayerHurt,
    };

    struct RunConfig final
    {
        std::uint32_t characterIndex{0U};
        std::uint32_t stageIndex{0U};
        float durationSeconds{180.0F};
        float moveSpeedMultiplier{1.0F};
        float damageMultiplier{1.0F};
        float fireIntervalMultiplier{1.0F};
        float maximumHealthMultiplier{1.0F};
        float enemyHealthMultiplier{1.0F};
        float enemySpeedMultiplier{1.0F};
        float spawnRateMultiplier{1.0F};
    };

    struct Enemy final
    {
        bool active{false};
        trace2d::scene::Vector2 position{};
        float health{0.0F};
        float maximumHealth{0.0F};
        float speed{0.0F};
        float radius{0.36F};
        EnemyKind kind{EnemyKind::Ghoul};
        std::uint8_t hitFlashFrames{0U};
        std::uint8_t orbitHitCooldownFrames{0U};
        std::uint32_t stableId{0U};
    };

    struct Projectile final
    {
        bool active{false};
        trace2d::scene::Vector2 position{};
        trace2d::scene::Vector2 velocity{};
        float damage{0.0F};
        float radius{0.16F};
        float lifetimeSeconds{0.0F};
        std::uint32_t stableId{0U};
    };

    struct Gem final
    {
        bool active{false};
        trace2d::scene::Vector2 position{};
        std::uint16_t value{1U};
        std::uint32_t stableId{0U};
    };

    struct Effect final
    {
        bool active{false};
        trace2d::scene::Vector2 position{};
        EffectKind kind{EffectKind::Hit};
        float ageSeconds{0.0F};
        float lifetimeSeconds{0.25F};
        float startScale{0.25F};
        float endScale{1.0F};
        std::uint32_t stableId{0U};
    };

    static constexpr float CanvasWidth = 960.0F;
    static constexpr float CanvasHeight = 540.0F;
    static constexpr float CameraVerticalSize = 9.0F;
    static constexpr float CameraHorizontalSize = 16.0F;
    static constexpr float PlayerRadius = 0.38F;
    static constexpr std::size_t MaximumEnemies = 384U;
    static constexpr std::size_t MaximumProjectiles = 128U;
    static constexpr std::size_t MaximumGems = 256U;
    static constexpr std::size_t MaximumEffects = 384U;

    NightfallSurvivorsGame(
        trace2d::audio::AudioComponentTypes2D audioTypes,
        std::filesystem::path runtimeRoot);

    void OnStart(trace2d::application::GameContext& context) override;
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override;
    void OnStop(trace2d::application::GameContext& context) override;

    void BeginRun(RunConfig config) noexcept;
    void ReturnToIdle() noexcept;
    void SetSfxVolume(float volume) noexcept;

    [[nodiscard]] State CurrentState() const noexcept;
    [[nodiscard]] const RunConfig& CurrentRunConfig() const noexcept;
    [[nodiscard]] float RunDurationSeconds() const noexcept;
    [[nodiscard]] trace2d::scene::EntityId Player() const noexcept;
    [[nodiscard]] trace2d::scene::Vector2 MoveIntent() const noexcept;
    [[nodiscard]] trace2d::scene::Vector2 Facing() const noexcept;
    [[nodiscard]] std::span<const Enemy> Enemies() const noexcept;
    [[nodiscard]] std::span<const Projectile> Projectiles() const noexcept;
    [[nodiscard]] std::span<const Gem> Gems() const noexcept;
    [[nodiscard]] std::span<const Effect> Effects() const noexcept;

    [[nodiscard]] std::uint32_t Health() const noexcept;
    [[nodiscard]] std::uint32_t MaximumHealth() const noexcept;
    [[nodiscard]] std::uint32_t Level() const noexcept;
    [[nodiscard]] std::uint32_t Experience() const noexcept;
    [[nodiscard]] std::uint32_t ExperienceToNextLevel() const noexcept;
    [[nodiscard]] std::uint32_t KillCount() const noexcept;
    [[nodiscard]] float ElapsedSeconds() const noexcept;
    [[nodiscard]] std::uint32_t RapidLevel() const noexcept;
    [[nodiscard]] std::uint32_t MightLevel() const noexcept;
    [[nodiscard]] std::uint32_t OrbitLevel() const noexcept;
    [[nodiscard]] std::uint64_t FrameCounter() const noexcept;
    [[nodiscard]] std::uint32_t PlayerFlashFrames() const noexcept;
    [[nodiscard]] std::uint32_t ScreenShakeFrames() const noexcept;

    [[nodiscard]] trace2d::audio::AudioSystem2D& Audio();
    [[nodiscard]] const trace2d::audio::AudioSystem2D& Audio() const;
    [[nodiscard]] trace2d::assets::ResourceRegistry& Resources() noexcept;
    [[nodiscard]] const trace2d::assets::ResourceRegistry& Resources() const noexcept;

private:
    void ResetRound(trace2d::application::GameContext& context);
    void UpdateRunning(
        trace2d::application::GameContext& context,
        float deltaSeconds);
    void SpawnEnemy(const trace2d::scene::Vector2& playerPosition);
    void FireVolley(const trace2d::scene::Vector2& playerPosition);
    void SpawnProjectile(
        trace2d::scene::Vector2 position,
        trace2d::scene::Vector2 direction,
        float angleOffsetRadians);
    void DamageEnemy(std::size_t enemyIndex, float damage, trace2d::scene::Vector2 impactPosition);
    void KillEnemy(std::size_t enemyIndex);
    void SpawnGem(trace2d::scene::Vector2 position, std::uint16_t value);
    void SpawnEffect(
        EffectKind kind,
        trace2d::scene::Vector2 position,
        float lifetimeSeconds,
        float startScale,
        float endScale);
    void ApplyUpgrade(std::uint32_t choice);
    void PlaySfx(trace2d::scene::EntityId entity) noexcept;
    void PublishAudioClip(const char* reference);
    [[nodiscard]] trace2d::scene::EntityId CreateAudioSource(
        trace2d::application::GameContext& context,
        const char* semanticId,
        const char* reference,
        float volume);
    [[nodiscard]] std::uint32_t NextRandom() noexcept;
    [[nodiscard]] float Random01() noexcept;

    trace2d::audio::AudioComponentTypes2D audioTypes_{};
    trace2d::assets::ResourceRegistry resources_;
    std::unique_ptr<trace2d::audio::AudioSystem2D> audio_{};

    trace2d::scene::EntityId player_{};
    trace2d::scene::EntityId fireSfx_{};
    trace2d::scene::EntityId hitSfx_{};
    trace2d::scene::EntityId killSfx_{};
    trace2d::scene::EntityId levelSfx_{};
    trace2d::scene::EntityId hurtSfx_{};

    trace2d::input::Axis1DActionId horizontalAction_{};
    trace2d::input::Axis1DActionId verticalAction_{};
    trace2d::input::ButtonActionId restartAction_{};
    trace2d::input::ButtonActionId upgradeRapidAction_{};
    trace2d::input::ButtonActionId upgradeMightAction_{};
    trace2d::input::ButtonActionId upgradeOrbitAction_{};

    std::array<Enemy, MaximumEnemies> enemies_{};
    std::array<Projectile, MaximumProjectiles> projectiles_{};
    std::array<Gem, MaximumGems> gems_{};
    std::array<Effect, MaximumEffects> effects_{};

    RunConfig activeRun_{};
    RunConfig pendingRun_{};
    bool startRequested_{false};
    State state_{State::Idle};
    trace2d::scene::Vector2 moveIntent_{};
    trace2d::scene::Vector2 facing_{1.0F, 0.0F};
    std::uint32_t health_{100U};
    std::uint32_t maximumHealth_{100U};
    std::uint32_t level_{1U};
    std::uint32_t experience_{0U};
    std::uint32_t experienceToNextLevel_{10U};
    std::uint32_t killCount_{0U};
    std::uint32_t rapidLevel_{0U};
    std::uint32_t mightLevel_{0U};
    std::uint32_t orbitLevel_{0U};
    std::uint32_t playerFlashFrames_{0U};
    std::uint32_t screenShakeFrames_{0U};
    std::uint32_t invulnerabilityFrames_{0U};
    std::uint32_t nextStableId_{1U};
    std::uint32_t randomState_{0xA341316CU};
    std::uint64_t frameCounter_{0U};
    float elapsedSeconds_{0.0F};
    float enemySpawnAccumulator_{0.0F};
    float fireCooldownSeconds_{0.0F};
    std::uint32_t lastHitSfxFrame_{0U};
    std::uint32_t lastKillSfxFrame_{0U};
};
