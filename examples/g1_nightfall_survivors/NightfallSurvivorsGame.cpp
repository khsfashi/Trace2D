#include "NightfallSurvivorsGame.hpp"

#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
constexpr float Pi = 3.14159265358979323846F;
constexpr float PlayerMoveSpeed = 4.2F;
constexpr float ProjectileSpeed = 9.0F;
constexpr float ProjectileLifetimeSeconds = 1.55F;
constexpr float BaseProjectileDamage = 2.2F;
constexpr float PlayerContactDamage = 12.0F;
constexpr std::uint32_t InvulnerabilityFrames = 42U;

constexpr char FireClip[] = "audio/laser.mp3";
constexpr char HitClip[] = "audio/brick-hit.mp3";
constexpr char KillClip[] = "audio/brick-break.mp3";
constexpr char LevelClip[] = "audio/powerup-get.mp3";
constexpr char HurtClip[] = "audio/life-lost.mp3";

[[nodiscard]] float DistanceSquared(
    const trace2d::scene::Vector2 a,
    const trace2d::scene::Vector2 b) noexcept
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

[[nodiscard]] trace2d::scene::Vector2 NormalizeOr(
    const trace2d::scene::Vector2 value,
    const trace2d::scene::Vector2 fallback) noexcept
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (lengthSquared <= 0.000001F) return fallback;
    const float inverseLength = 1.0F / std::sqrt(lengthSquared);
    return {value.x * inverseLength, value.y * inverseLength};
}

[[nodiscard]] trace2d::scene::Vector2 Rotate(
    const trace2d::scene::Vector2 value,
    const float radians) noexcept
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return {
        value.x * cosine - value.y * sine,
        value.x * sine + value.y * cosine,
    };
}

[[nodiscard]] ma_result OpenDecoder(const std::filesystem::path& path, ma_decoder& decoder) noexcept
{
#if defined(_WIN32)
    return ma_decoder_init_file_w(path.c_str(), nullptr, &decoder);
#else
    const std::string native = path.string();
    return ma_decoder_init_file(native.c_str(), nullptr, &decoder);
#endif
}
} // namespace

NightfallSurvivorsGame::NightfallSurvivorsGame(
    const trace2d::audio::AudioComponentTypes2D audioTypes,
    std::filesystem::path runtimeRoot)
    : audioTypes_{audioTypes}
    , resources_{std::move(runtimeRoot)}
{
}

void NightfallSurvivorsGame::PublishAudioClip(const char* const reference)
{
    const std::filesystem::path path = resources_.ProjectRoot() / reference;
    std::error_code fileError{};
    const std::uint64_t encodedBytes = static_cast<std::uint64_t>(std::filesystem::file_size(path, fileError));
    if (fileError || encodedBytes == 0U)
        throw std::runtime_error{"Nightfall Survivors audio asset is unavailable: " + std::string{reference}};

    ma_decoder decoder{};
    const ma_result openResult = OpenDecoder(path, decoder);
    if (openResult != MA_SUCCESS)
        throw std::runtime_error{"Nightfall Survivors could not inspect downloaded audio: " + std::string{reference}};

    ma_uint64 frameCount = 0U;
    const ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    const ma_uint32 channels = decoder.outputChannels;
    const ma_uint32 sampleRate = decoder.outputSampleRate;
    ma_decoder_uninit(&decoder);
    if (lengthResult != MA_SUCCESS || frameCount == 0U || channels == 0U || sampleRate == 0U)
        throw std::runtime_error{"Nightfall Survivors could not derive canonical audio metadata."};

    trace2d::assets::AudioClipResource clip{};
    clip.loadPolicy = trace2d::assets::AudioClipLoadPolicy::Preload;
    clip.sampleRateHz = sampleRate;
    clip.channelCount = static_cast<std::uint16_t>(channels);
    clip.frameCount = frameCount;
    clip.encodedByteSize = encodedBytes;
    const auto published = resources_.PublishAudioClip(reference, std::move(clip));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not publish canonical audio metadata."};
}

trace2d::scene::EntityId NightfallSurvivorsGame::CreateAudioSource(
    trace2d::application::GameContext& context,
    const char* const semanticId,
    const char* const reference,
    const float volume)
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = semanticId;
    descriptor.name = semanticId;
    const trace2d::scene::EntityId entity = context.Scene().CreateEntity(std::move(descriptor));

    trace2d::audio::AudioSource2D source{};
    source.clipReference = reference;
    source.volume = volume;
    source.pitch = 1.0F;
    source.group = trace2d::audio::AudioGroup2D::Sfx;
    static_cast<void>(context.Scene().AddComponent(entity, audioTypes_.source, std::move(source)));
    return entity;
}

void NightfallSurvivorsGame::OnStart(trace2d::application::GameContext& context)
{
    horizontalAction_ = context.Actions().AddAxis1DAction(
        "survivors.move.horizontal",
        trace2d::input::InputControl::KeyA,
        trace2d::input::InputControl::KeyD);
    verticalAction_ = context.Actions().AddAxis1DAction(
        "survivors.move.vertical",
        trace2d::input::InputControl::KeyS,
        trace2d::input::InputControl::KeyW);
    restartAction_ = context.Actions().AddButtonAction("survivors.restart");
    context.Actions().BindButton(restartAction_, trace2d::input::InputControl::KeyR);
    upgradeRapidAction_ = context.Actions().AddButtonAction("survivors.upgrade.rapid");
    context.Actions().BindButton(upgradeRapidAction_, trace2d::input::InputControl::KeyQ);
    upgradeMightAction_ = context.Actions().AddButtonAction("survivors.upgrade.might");
    context.Actions().BindButton(upgradeMightAction_, trace2d::input::InputControl::KeyE);
    upgradeOrbitAction_ = context.Actions().AddButtonAction("survivors.upgrade.orbit");
    context.Actions().BindButton(upgradeOrbitAction_, trace2d::input::InputControl::KeyF);

    trace2d::scene::EntityDescriptor playerDescriptor{};
    playerDescriptor.semanticId = "survivors.player";
    playerDescriptor.name = "Warden";
    player_ = context.Scene().CreateEntity(std::move(playerDescriptor));

    PublishAudioClip(FireClip);
    PublishAudioClip(HitClip);
    PublishAudioClip(KillClip);
    PublishAudioClip(LevelClip);
    PublishAudioClip(HurtClip);

    fireSfx_ = CreateAudioSource(context, "survivors.audio.fire", FireClip, 0.22F);
    hitSfx_ = CreateAudioSource(context, "survivors.audio.hit", HitClip, 0.18F);
    killSfx_ = CreateAudioSource(context, "survivors.audio.kill", KillClip, 0.28F);
    levelSfx_ = CreateAudioSource(context, "survivors.audio.level", LevelClip, 0.55F);
    hurtSfx_ = CreateAudioSource(context, "survivors.audio.hurt", HurtClip, 0.50F);

    audio_ = std::make_unique<trace2d::audio::AudioSystem2D>(context.Scene(), resources_, audioTypes_.source);
    if (!audio_->ReserveVoices(32U) || !audio_->ReserveEvents(256U))
        throw std::runtime_error{"Nightfall Survivors could not reserve audio storage."};
    trace2d::audio::AudioVoiceLimits2D limits{};
    limits.globalLimit = 32U;
    limits.groupLimits[static_cast<std::size_t>(trace2d::audio::AudioGroup2D::Sfx)] = 32U;
    limits.overflowPolicy = trace2d::audio::AudioVoiceOverflowPolicy2D::StealOldest;
    if (!audio_->SetVoiceLimits(limits))
        throw std::runtime_error{"Nightfall Survivors could not set audio voice limits."};

    ResetRound(context);
}

void NightfallSurvivorsGame::ResetRound(trace2d::application::GameContext& context)
{
    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    if (player == nullptr) throw std::runtime_error{"Nightfall Survivors lost the canonical player."};
    player->LocalTransform().position = {};

    enemies_.fill({});
    projectiles_.fill({});
    gems_.fill({});
    effects_.fill({});

    state_ = State::Running;
    moveIntent_ = {};
    facing_ = {1.0F, 0.0F};
    health_ = 100U;
    maximumHealth_ = 100U;
    level_ = 1U;
    experience_ = 0U;
    experienceToNextLevel_ = 10U;
    killCount_ = 0U;
    rapidLevel_ = 0U;
    mightLevel_ = 0U;
    orbitLevel_ = 0U;
    playerFlashFrames_ = 0U;
    screenShakeFrames_ = 0U;
    invulnerabilityFrames_ = 0U;
    nextStableId_ = 1U;
    randomState_ = 0xA341316CU;
    frameCounter_ = 0U;
    elapsedSeconds_ = 0.0F;
    enemySpawnAccumulator_ = 0.0F;
    fireCooldownSeconds_ = 0.20F;
    lastHitSfxFrame_ = 0U;
    lastKillSfxFrame_ = 0U;

    for (std::size_t index = 0U; index < 12U; ++index)
        SpawnEnemy(player->LocalTransform().position);
}

std::uint32_t NightfallSurvivorsGame::NextRandom() noexcept
{
    std::uint32_t value = randomState_;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    randomState_ = value == 0U ? 0x9E3779B9U : value;
    return randomState_;
}

float NightfallSurvivorsGame::Random01() noexcept
{
    return static_cast<float>(NextRandom() & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
}

void NightfallSurvivorsGame::PlaySfx(const trace2d::scene::EntityId entity) noexcept
{
    if (audio_ == nullptr) return;
    static_cast<void>(audio_->Play(entity));
}

void NightfallSurvivorsGame::SpawnEnemy(const trace2d::scene::Vector2& playerPosition)
{
    const auto slot = std::find_if(enemies_.begin(), enemies_.end(), [](const Enemy& enemy) { return !enemy.active; });
    if (slot == enemies_.end()) return;

    const float angle = Random01() * 2.0F * Pi;
    const float radius = 7.5F + Random01() * 3.2F;
    const float difficulty = std::clamp(elapsedSeconds_ / RunDurationSeconds, 0.0F, 1.0F);
    const float roll = Random01();

    EnemyKind kind = EnemyKind::Ghoul;
    if (elapsedSeconds_ > 70.0F && roll > 0.78F) kind = EnemyKind::Wisp;
    else if (elapsedSeconds_ > 25.0F && roll > 0.62F) kind = EnemyKind::Brute;

    float health = 2.5F + difficulty * 9.0F;
    float speed = 0.88F + difficulty * 0.52F;
    float enemyRadius = 0.36F;
    if (kind == EnemyKind::Brute)
    {
        health *= 2.3F;
        speed *= 0.72F;
        enemyRadius = 0.47F;
    }
    else if (kind == EnemyKind::Wisp)
    {
        health *= 0.75F;
        speed *= 1.45F;
        enemyRadius = 0.30F;
    }

    *slot = Enemy{
        .active = true,
        .position = {
            playerPosition.x + std::cos(angle) * radius,
            playerPosition.y + std::sin(angle) * radius,
        },
        .health = health,
        .maximumHealth = health,
        .speed = speed,
        .radius = enemyRadius,
        .kind = kind,
        .stableId = nextStableId_++,
    };
}

void NightfallSurvivorsGame::SpawnProjectile(
    const trace2d::scene::Vector2 position,
    trace2d::scene::Vector2 direction,
    const float angleOffsetRadians)
{
    const auto slot = std::find_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& projectile) {
        return !projectile.active;
    });
    if (slot == projectiles_.end()) return;

    direction = NormalizeOr(direction, facing_);
    direction = Rotate(direction, angleOffsetRadians);
    const float damage = BaseProjectileDamage * (1.0F + static_cast<float>(mightLevel_) * 0.34F);
    *slot = Projectile{
        .active = true,
        .position = position,
        .velocity = {direction.x * ProjectileSpeed, direction.y * ProjectileSpeed},
        .damage = damage,
        .radius = 0.16F,
        .lifetimeSeconds = ProjectileLifetimeSeconds,
        .stableId = nextStableId_++,
    };
}

void NightfallSurvivorsGame::FireVolley(const trace2d::scene::Vector2& playerPosition)
{
    const Enemy* target = nullptr;
    float nearestDistanceSquared = 14.0F * 14.0F;
    for (const Enemy& enemy : enemies_)
    {
        if (!enemy.active) continue;
        const float distanceSquared = DistanceSquared(playerPosition, enemy.position);
        if (distanceSquared < nearestDistanceSquared)
        {
            nearestDistanceSquared = distanceSquared;
            target = &enemy;
        }
    }
    if (target == nullptr) return;

    const trace2d::scene::Vector2 direction = NormalizeOr(
        {target->position.x - playerPosition.x, target->position.y - playerPosition.y},
        facing_);
    const std::uint32_t projectileCount = std::min<std::uint32_t>(3U, 1U + rapidLevel_ / 3U);
    if (projectileCount == 1U)
    {
        SpawnProjectile(playerPosition, direction, 0.0F);
    }
    else if (projectileCount == 2U)
    {
        SpawnProjectile(playerPosition, direction, -0.10F);
        SpawnProjectile(playerPosition, direction, 0.10F);
    }
    else
    {
        SpawnProjectile(playerPosition, direction, -0.16F);
        SpawnProjectile(playerPosition, direction, 0.0F);
        SpawnProjectile(playerPosition, direction, 0.16F);
    }
    PlaySfx(fireSfx_);
}

void NightfallSurvivorsGame::SpawnGem(
    const trace2d::scene::Vector2 position,
    const std::uint16_t value)
{
    const auto slot = std::find_if(gems_.begin(), gems_.end(), [](const Gem& gem) { return !gem.active; });
    if (slot == gems_.end()) return;
    *slot = Gem{
        .active = true,
        .position = position,
        .value = value,
        .stableId = nextStableId_++,
    };
}

void NightfallSurvivorsGame::SpawnEffect(
    const EffectKind kind,
    const trace2d::scene::Vector2 position,
    const float lifetimeSeconds,
    const float startScale,
    const float endScale)
{
    const auto slot = std::find_if(effects_.begin(), effects_.end(), [](const Effect& effect) { return !effect.active; });
    if (slot == effects_.end()) return;
    *slot = Effect{
        .active = true,
        .position = position,
        .kind = kind,
        .ageSeconds = 0.0F,
        .lifetimeSeconds = lifetimeSeconds,
        .startScale = startScale,
        .endScale = endScale,
        .stableId = nextStableId_++,
    };
}

void NightfallSurvivorsGame::KillEnemy(const std::size_t enemyIndex)
{
    Enemy& enemy = enemies_[enemyIndex];
    if (!enemy.active) return;
    const trace2d::scene::Vector2 position = enemy.position;
    const std::uint16_t gemValue = enemy.kind == EnemyKind::Brute ? 3U : (enemy.kind == EnemyKind::Wisp ? 2U : 1U);
    enemy.active = false;
    ++killCount_;
    SpawnGem(position, gemValue);
    SpawnEffect(EffectKind::Death, position, 0.34F, 0.35F, 1.45F);
    if (frameCounter_ - lastKillSfxFrame_ >= 5U)
    {
        PlaySfx(killSfx_);
        lastKillSfxFrame_ = static_cast<std::uint32_t>(frameCounter_);
    }
}

void NightfallSurvivorsGame::DamageEnemy(
    const std::size_t enemyIndex,
    const float damage,
    const trace2d::scene::Vector2 impactPosition)
{
    Enemy& enemy = enemies_[enemyIndex];
    if (!enemy.active) return;
    enemy.health -= damage;
    enemy.hitFlashFrames = 5U;
    SpawnEffect(EffectKind::Hit, impactPosition, 0.16F, 0.18F, 0.62F);
    if (frameCounter_ - lastHitSfxFrame_ >= 3U)
    {
        PlaySfx(hitSfx_);
        lastHitSfxFrame_ = static_cast<std::uint32_t>(frameCounter_);
    }
    if (enemy.health <= 0.0F) KillEnemy(enemyIndex);
}

void NightfallSurvivorsGame::ApplyUpgrade(const std::uint32_t choice)
{
    switch (choice)
    {
    case 0U:
        rapidLevel_ = std::min<std::uint32_t>(rapidLevel_ + 1U, 9U);
        break;
    case 1U:
        mightLevel_ = std::min<std::uint32_t>(mightLevel_ + 1U, 9U);
        break;
    case 2U:
        if (orbitLevel_ < 4U)
        {
            ++orbitLevel_;
        }
        else
        {
            maximumHealth_ += 10U;
            health_ = std::min(maximumHealth_, health_ + 20U);
        }
        break;
    default:
        break;
    }
    state_ = State::Running;
}

void NightfallSurvivorsGame::UpdateRunning(
    trace2d::application::GameContext& context,
    const float deltaSeconds)
{
    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    if (player == nullptr) throw std::runtime_error{"Nightfall Survivors lost the canonical player."};

    elapsedSeconds_ += deltaSeconds;
    if (elapsedSeconds_ >= RunDurationSeconds)
    {
        state_ = State::Won;
        SpawnEffect(EffectKind::LevelUp, player->LocalTransform().position, 1.0F, 0.5F, 4.0F);
        PlaySfx(levelSfx_);
        return;
    }

    const float horizontal = context.Actions().Axis1D(horizontalAction_);
    const float vertical = context.Actions().Axis1D(verticalAction_);
    const trace2d::scene::Vector2 normalized = NormalizeOr({horizontal, vertical}, {});
    if (horizontal != 0.0F || vertical != 0.0F)
    {
        moveIntent_ = normalized;
        facing_ = normalized;
    }
    else
    {
        moveIntent_ = {};
    }

    auto& playerPosition = player->LocalTransform().position;
    playerPosition.x += moveIntent_.x * PlayerMoveSpeed * deltaSeconds;
    playerPosition.y += moveIntent_.y * PlayerMoveSpeed * deltaSeconds;

    if (invulnerabilityFrames_ > 0U) --invulnerabilityFrames_;
    if (playerFlashFrames_ > 0U) --playerFlashFrames_;
    if (screenShakeFrames_ > 0U) --screenShakeFrames_;

    const float difficulty = std::clamp(elapsedSeconds_ / RunDurationSeconds, 0.0F, 1.0F);
    const float spawnInterval = std::max(0.105F, 0.48F - difficulty * 0.375F);
    enemySpawnAccumulator_ += deltaSeconds;
    while (enemySpawnAccumulator_ >= spawnInterval)
    {
        enemySpawnAccumulator_ -= spawnInterval;
        SpawnEnemy(playerPosition);
        if (elapsedSeconds_ > 95.0F && Random01() > 0.68F) SpawnEnemy(playerPosition);
    }

    fireCooldownSeconds_ -= deltaSeconds;
    if (fireCooldownSeconds_ <= 0.0F)
    {
        FireVolley(playerPosition);
        const float rapidMultiplier = std::pow(0.88F, static_cast<float>(rapidLevel_));
        fireCooldownSeconds_ = std::max(0.14F, 0.54F * rapidMultiplier);
    }

    for (Enemy& enemy : enemies_)
    {
        if (!enemy.active) continue;
        if (enemy.hitFlashFrames > 0U) --enemy.hitFlashFrames;
        if (enemy.orbitHitCooldownFrames > 0U) --enemy.orbitHitCooldownFrames;

        const trace2d::scene::Vector2 direction = NormalizeOr(
            {playerPosition.x - enemy.position.x, playerPosition.y - enemy.position.y},
            {});
        enemy.position.x += direction.x * enemy.speed * deltaSeconds;
        enemy.position.y += direction.y * enemy.speed * deltaSeconds;
    }

    for (Projectile& projectile : projectiles_)
    {
        if (!projectile.active) continue;
        projectile.position.x += projectile.velocity.x * deltaSeconds;
        projectile.position.y += projectile.velocity.y * deltaSeconds;
        projectile.lifetimeSeconds -= deltaSeconds;
        if (projectile.lifetimeSeconds <= 0.0F)
        {
            projectile.active = false;
            continue;
        }

        for (std::size_t enemyIndex = 0U; enemyIndex < enemies_.size(); ++enemyIndex)
        {
            const Enemy& enemy = enemies_[enemyIndex];
            if (!enemy.active) continue;
            const float radius = projectile.radius + enemy.radius;
            if (DistanceSquared(projectile.position, enemy.position) > radius * radius) continue;
            DamageEnemy(enemyIndex, projectile.damage, projectile.position);
            projectile.active = false;
            break;
        }
    }

    const std::uint32_t orbitCount = std::min<std::uint32_t>(orbitLevel_, 4U);
    if (orbitCount > 0U)
    {
        const float orbitRadius = 1.25F + static_cast<float>(orbitCount) * 0.08F;
        const float baseAngle = elapsedSeconds_ * 3.4F;
        for (std::uint32_t blade = 0U; blade < orbitCount; ++blade)
        {
            const float angle = baseAngle + 2.0F * Pi * static_cast<float>(blade) / static_cast<float>(orbitCount);
            const trace2d::scene::Vector2 bladePosition{
                playerPosition.x + std::cos(angle) * orbitRadius,
                playerPosition.y + std::sin(angle) * orbitRadius,
            };
            for (std::size_t enemyIndex = 0U; enemyIndex < enemies_.size(); ++enemyIndex)
            {
                const Enemy& enemy = enemies_[enemyIndex];
                if (!enemy.active || enemy.orbitHitCooldownFrames > 0U) continue;
                const float hitRadius = enemy.radius + 0.28F;
                if (DistanceSquared(bladePosition, enemy.position) > hitRadius * hitRadius) continue;
                enemies_[enemyIndex].orbitHitCooldownFrames = 15U;
                DamageEnemy(enemyIndex, 1.35F * (1.0F + static_cast<float>(mightLevel_) * 0.22F), bladePosition);
            }
        }
    }

    if (invulnerabilityFrames_ == 0U)
    {
        for (Enemy& enemy : enemies_)
        {
            if (!enemy.active) continue;
            const float contactRadius = PlayerRadius + enemy.radius;
            if (DistanceSquared(playerPosition, enemy.position) > contactRadius * contactRadius) continue;

            const std::uint32_t damage = static_cast<std::uint32_t>(PlayerContactDamage);
            health_ = health_ > damage ? health_ - damage : 0U;
            invulnerabilityFrames_ = InvulnerabilityFrames;
            playerFlashFrames_ = 10U;
            screenShakeFrames_ = 10U;
            SpawnEffect(EffectKind::PlayerHurt, playerPosition, 0.38F, 0.40F, 1.45F);
            PlaySfx(hurtSfx_);

            const trace2d::scene::Vector2 push = NormalizeOr(
                {enemy.position.x - playerPosition.x, enemy.position.y - playerPosition.y},
                {1.0F, 0.0F});
            enemy.position.x += push.x * 0.85F;
            enemy.position.y += push.y * 0.85F;
            if (health_ == 0U) state_ = State::Lost;
            break;
        }
    }

    const float magnetRadius = 1.35F + static_cast<float>(level_) * 0.035F;
    for (Gem& gem : gems_)
    {
        if (!gem.active) continue;
        const float distanceSquared = DistanceSquared(gem.position, playerPosition);
        if (distanceSquared <= magnetRadius * magnetRadius)
        {
            const trace2d::scene::Vector2 direction = NormalizeOr(
                {playerPosition.x - gem.position.x, playerPosition.y - gem.position.y},
                {});
            gem.position.x += direction.x * 7.2F * deltaSeconds;
            gem.position.y += direction.y * 7.2F * deltaSeconds;
        }
        if (DistanceSquared(gem.position, playerPosition) <= 0.48F * 0.48F)
        {
            experience_ += gem.value;
            gem.active = false;
        }
    }

    for (Effect& effect : effects_)
    {
        if (!effect.active) continue;
        effect.ageSeconds += deltaSeconds;
        if (effect.ageSeconds >= effect.lifetimeSeconds) effect.active = false;
    }

    if (state_ == State::Running && experience_ >= experienceToNextLevel_)
    {
        experience_ -= experienceToNextLevel_;
        ++level_;
        experienceToNextLevel_ = 10U + level_ * 5U;
        state_ = State::LevelUp;
        SpawnEffect(EffectKind::LevelUp, playerPosition, 0.90F, 0.45F, 3.8F);
        PlaySfx(levelSfx_);
    }
}

void NightfallSurvivorsGame::OnFixedUpdate(
    trace2d::application::GameContext& context,
    const trace2d::application::FixedUpdate& update)
{
    ++frameCounter_;

    if (context.Actions().Pressed(restartAction_))
    {
        ResetRound(context);
    }
    else if (state_ == State::LevelUp)
    {
        if (context.Actions().Pressed(upgradeRapidAction_)) ApplyUpgrade(0U);
        else if (context.Actions().Pressed(upgradeMightAction_)) ApplyUpgrade(1U);
        else if (context.Actions().Pressed(upgradeOrbitAction_)) ApplyUpgrade(2U);
    }
    else if (state_ == State::Running)
    {
        UpdateRunning(context, std::chrono::duration<float>{update.fixedDelta}.count());
    }
    else
    {
        for (Effect& effect : effects_)
        {
            if (!effect.active) continue;
            effect.ageSeconds += std::chrono::duration<float>{update.fixedDelta}.count();
            if (effect.ageSeconds >= effect.lifetimeSeconds) effect.active = false;
        }
    }

    if (audio_ == nullptr || audio_->Step(update.fixedDelta).result != trace2d::audio::AudioStepResult2D::Success)
        throw std::runtime_error{"Nightfall Survivors semantic audio fixed step failed."};
}

void NightfallSurvivorsGame::OnStop(trace2d::application::GameContext&)
{
}

NightfallSurvivorsGame::State NightfallSurvivorsGame::CurrentState() const noexcept { return state_; }
trace2d::scene::EntityId NightfallSurvivorsGame::Player() const noexcept { return player_; }
trace2d::scene::Vector2 NightfallSurvivorsGame::MoveIntent() const noexcept { return moveIntent_; }
trace2d::scene::Vector2 NightfallSurvivorsGame::Facing() const noexcept { return facing_; }
std::span<const NightfallSurvivorsGame::Enemy> NightfallSurvivorsGame::Enemies() const noexcept { return enemies_; }
std::span<const NightfallSurvivorsGame::Projectile> NightfallSurvivorsGame::Projectiles() const noexcept { return projectiles_; }
std::span<const NightfallSurvivorsGame::Gem> NightfallSurvivorsGame::Gems() const noexcept { return gems_; }
std::span<const NightfallSurvivorsGame::Effect> NightfallSurvivorsGame::Effects() const noexcept { return effects_; }
std::uint32_t NightfallSurvivorsGame::Health() const noexcept { return health_; }
std::uint32_t NightfallSurvivorsGame::MaximumHealth() const noexcept { return maximumHealth_; }
std::uint32_t NightfallSurvivorsGame::Level() const noexcept { return level_; }
std::uint32_t NightfallSurvivorsGame::Experience() const noexcept { return experience_; }
std::uint32_t NightfallSurvivorsGame::ExperienceToNextLevel() const noexcept { return experienceToNextLevel_; }
std::uint32_t NightfallSurvivorsGame::KillCount() const noexcept { return killCount_; }
float NightfallSurvivorsGame::ElapsedSeconds() const noexcept { return elapsedSeconds_; }
std::uint32_t NightfallSurvivorsGame::RapidLevel() const noexcept { return rapidLevel_; }
std::uint32_t NightfallSurvivorsGame::MightLevel() const noexcept { return mightLevel_; }
std::uint32_t NightfallSurvivorsGame::OrbitLevel() const noexcept { return orbitLevel_; }
std::uint64_t NightfallSurvivorsGame::FrameCounter() const noexcept { return frameCounter_; }
std::uint32_t NightfallSurvivorsGame::PlayerFlashFrames() const noexcept { return playerFlashFrames_; }
std::uint32_t NightfallSurvivorsGame::ScreenShakeFrames() const noexcept { return screenShakeFrames_; }

trace2d::audio::AudioSystem2D& NightfallSurvivorsGame::Audio()
{
    if (audio_ == nullptr) throw std::runtime_error{"Nightfall Survivors audio requested before startup."};
    return *audio_;
}

const trace2d::audio::AudioSystem2D& NightfallSurvivorsGame::Audio() const
{
    if (audio_ == nullptr) throw std::runtime_error{"Nightfall Survivors audio requested before startup."};
    return *audio_;
}

trace2d::assets::ResourceRegistry& NightfallSurvivorsGame::Resources() noexcept { return resources_; }
const trace2d::assets::ResourceRegistry& NightfallSurvivorsGame::Resources() const noexcept { return resources_; }
