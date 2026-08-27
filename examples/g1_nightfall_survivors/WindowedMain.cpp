#include "NightfallSurvivorsGame.hpp"

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/assets/TextureAssets.hpp>
#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>
#include <trace2d/render/SpriteRenderContract.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>
#include <trace2d/scene/SpriteTransform2D.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef TRACE2D_G1_RUNTIME_DIR
#define TRACE2D_G1_RUNTIME_DIR "."
#endif

namespace
{
using Clock = std::chrono::steady_clock;
using TextureHandle = trace2d::render::TextureHandle;
using Color = trace2d::render::SpriteLinearRgba;
constexpr float Pi = 3.14159265358979323846F;

struct RegistryTypes final
{
    trace2d::scene::ComponentRegistry registry{};
    trace2d::audio::AudioComponentTypes2D audio{};

    RegistryTypes()
        : audio{trace2d::audio::RegisterAudio2DComponents(registry)}
    {
        registry.Freeze();
    }
};

struct Visual final
{
    trace2d::assets::SpriteAsset asset{};
    std::vector<trace2d::render::ResolvedSpriteRegion> selections{};
    TextureHandle texture{};
    float pixelsPerUnit{1.0F};
};

struct PresentationState final
{
    trace2d::render::Renderer* renderer{nullptr};
    NightfallSurvivorsGame* game{nullptr};
    Visual floor{};
    Visual floorAlt{};
    Visual hero{};
    Visual ghoul{};
    Visual brute{};
    Visual particle{};
    Visual white{};
    std::array<trace2d::runtime::SpriteAnimationClip2D, 4U> walkClips{};
    std::array<trace2d::runtime::SpriteAnimator2D, 4U> walkAnimators{};
    std::vector<trace2d::render::SpritePresentationRenderData> draws{};
    std::uint64_t lastAnimationFrame{0U};
};

[[nodiscard]] trace2d::application::ApplicationConfig MakeConfig()
{
    trace2d::application::ApplicationConfig config{};
    config.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
    config.runtime.seed = 417U;
    config.scene.semanticId = "g1.nightfall-survivors";
    config.scene.name = "Nightfall Survivors";
    config.uiWidth = static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasWidth);
    config.uiHeight = static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasHeight);
    return config;
}

[[nodiscard]] Visual LoadVisual(
    trace2d::assets::TextureAssetCache& cache,
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const std::string& reference,
    const std::uint32_t columns,
    const std::uint32_t rows,
    const float pixelsPerUnit)
{
    const trace2d::assets::TextureAssetLoadResult loaded = cache.Load(reference);
    if (!loaded.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not load web asset: " + reference};
    if (columns == 0U || rows == 0U || loaded.asset->width % columns != 0U || loaded.asset->height % rows != 0U)
        throw std::runtime_error{"Nightfall Survivors sprite grid does not divide the source texture."};

    trace2d::assets::TextureResource canonical{};
    canonical.width = loaded.asset->width;
    canonical.height = loaded.asset->height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Srgb;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors owner-playable web asset";
    canonical.canonicalRgba8 = loaded.asset->rgba8;
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not publish a presentation texture."};

    trace2d::render::Rgba8TextureData gpuData{};
    gpuData.width = loaded.asset->width;
    gpuData.height = loaded.asset->height;
    gpuData.pixels = std::span<const std::uint8_t>{loaded.asset->rgba8.data(), loaded.asset->rgba8.size()};
    const TextureHandle texture = renderer.CreateSpriteTextureRgba8(
        published.handle,
        gpuData,
        trace2d::render::SpriteTextureEncoding::Srgb);

    Visual visual{};
    visual.texture = texture;
    visual.pixelsPerUnit = pixelsPerUnit;
    visual.asset.id = reference + ".sprite";
    visual.asset.sampling = trace2d::assets::SpriteSampling::Nearest;

    trace2d::assets::SpriteAtlasPage page{};
    page.id = "page";
    page.textureReference = reference;
    page.size = {loaded.asset->width, loaded.asset->height};
    page.colorSpace = trace2d::assets::SpriteColorSpace::Srgb;
    visual.asset.pages.push_back(std::move(page));

    const std::uint32_t cellWidth = loaded.asset->width / columns;
    const std::uint32_t cellHeight = loaded.asset->height / rows;
    visual.asset.regions.reserve(static_cast<std::size_t>(columns) * rows);
    for (std::uint32_t row = 0U; row < rows; ++row)
    {
        for (std::uint32_t column = 0U; column < columns; ++column)
        {
            trace2d::assets::SpriteRegion region{};
            region.id = "region-" + std::to_string(row * columns + column);
            region.pageId = "page";
            region.sourceSize = {cellWidth, cellHeight};
            region.trimOffset = {0U, 0U};
            region.trimSize = {cellWidth, cellHeight};
            region.packedRect = {column * cellWidth, row * cellHeight, cellWidth, cellHeight};
            region.pivot = {
                static_cast<std::int64_t>(cellWidth),
                static_cast<std::int64_t>(cellHeight),
                2,
            };
            visual.asset.regions.push_back(std::move(region));
        }
    }

    visual.selections.resize(visual.asset.regions.size());
    for (std::size_t index = 0U; index < visual.asset.regions.size(); ++index)
    {
        if (!trace2d::render::ResolveSpriteRegionByIndices(
                &visual.asset, 0U, index, visual.selections[index]).Succeeded())
            throw std::runtime_error{"Nightfall Survivors could not resolve a prepared sprite region."};
    }
    return visual;
}

[[nodiscard]] Visual CreateWhiteVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer)
{
    constexpr std::array<std::uint8_t, 4U> White{255U, 255U, 255U, 255U};
    const std::string reference = "generated/white.rgba8";
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors generated HUD primitive";
    canonical.canonicalRgba8.assign(White.begin(), White.end());
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"Nightfall Survivors could not publish white texture."};

    trace2d::render::Rgba8TextureData data{};
    data.width = 1U;
    data.height = 1U;
    data.pixels = White;

    Visual visual{};
    visual.texture = renderer.CreateTextureRgba8(published.handle, data);
    visual.pixelsPerUnit = 1.0F;
    visual.asset.id = "generated-white.sprite";
    visual.asset.sampling = trace2d::assets::SpriteSampling::Nearest;
    visual.asset.pages.push_back({
        .id = "page",
        .textureReference = reference,
        .size = {1U, 1U},
        .colorSpace = trace2d::assets::SpriteColorSpace::Linear,
    });
    trace2d::assets::SpriteRegion region{};
    region.id = "region-0";
    region.pageId = "page";
    region.sourceSize = {1U, 1U};
    region.trimSize = {1U, 1U};
    region.packedRect = {0U, 0U, 1U, 1U};
    region.pivot = {1, 1, 2};
    visual.asset.regions.push_back(std::move(region));
    visual.selections.resize(1U);
    if (!trace2d::render::ResolveSpriteRegionByIndices(&visual.asset, 0U, 0U, visual.selections[0]).Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not resolve white visual."};
    return visual;
}

void PrepareWalkAnimation(PresentationState& state)
{
    constexpr auto FrameDuration = std::chrono::milliseconds{70};
    for (std::uint32_t row = 0U; row < 4U; ++row)
    {
        std::array<trace2d::runtime::SpriteAnimationFrame2D, 8U> frames{};
        for (std::uint32_t frame = 0U; frame < frames.size(); ++frame)
        {
            frames[frame].regionIndex = row * 8U + frame;
            frames[frame].duration = FrameDuration;
        }
        if (!trace2d::runtime::SpriteAnimationClip2D::Prepare(
                &state.hero.asset,
                static_cast<std::uint32_t>(state.hero.asset.regions.size()),
                frames,
                state.walkClips[row]).Succeeded())
            throw std::runtime_error{"Nightfall Survivors could not prepare the CC0 walk cycle."};

        trace2d::runtime::SpriteAnimator2DState animatorState{};
        if (!trace2d::runtime::MakeSpriteAnimator2DState(
                state.walkClips[row],
                {},
                trace2d::runtime::SpriteAnimationPlaybackState::Playing,
                trace2d::runtime::SpriteAnimationLoopMode::Loop,
                trace2d::runtime::SpriteAnimationDirection::Forward,
                false,
                {1U, 1U},
                animatorState).Succeeded() ||
            !state.walkAnimators[row].RestoreState(animatorState).Succeeded())
            throw std::runtime_error{"Nightfall Survivors could not initialize a walk animator."};
    }
}

void AdvanceWalkAnimation(PresentationState& state)
{
    const std::uint64_t currentFrame = state.game->FrameCounter();
    if (currentFrame <= state.lastAnimationFrame) return;
    const std::uint64_t deltaFrames = currentFrame - state.lastAnimationFrame;
    state.lastAnimationFrame = currentFrame;
    const auto delta = std::chrono::nanoseconds{
        static_cast<std::int64_t>(deltaFrames) * 16'666'667LL};
    std::array<trace2d::runtime::SpriteAnimationEmission2D, 32U> emissions{};
    for (auto& animator : state.walkAnimators)
    {
        const auto result = animator.Advance(delta, emissions);
        if (!result.Succeeded()) throw std::runtime_error{"Nightfall Survivors walk animation advance failed."};
    }
}

void AddVisual(
    PresentationState& state,
    const Visual& visual,
    const std::size_t regionIndex,
    const trace2d::scene::Vector2 position,
    const trace2d::scene::Vector2 scale,
    const float rotation,
    const Color tint,
    const float opacity,
    const trace2d::render::SpriteBlendMode blend,
    const std::int32_t layer,
    std::uint64_t& order)
{
    if (regionIndex >= visual.selections.size()) throw std::runtime_error{"Nightfall Survivors region index overflow."};

    trace2d::scene::SpritePose2D pose{};
    pose.transform.position = position;
    pose.transform.rotationRadians = rotation;
    pose.transform.scale = scale;

    trace2d::render::SpriteAppearance2D appearance{};
    appearance.tint = tint;
    appearance.opacity = opacity;
    appearance.sampling = trace2d::render::SpriteAppearanceSampling::Nearest;
    appearance.blend = blend;

    trace2d::render::SpritePresentation2D presentation{};
    if (!trace2d::render::BuildSpritePresentation2D(
            visual.selections[regionIndex], pose, visual.pixelsPerUnit, appearance, presentation).Succeeded())
        throw std::runtime_error{"Nightfall Survivors sprite presentation build failed."};

    trace2d::render::SpritePresentationRenderData draw{};
    draw.presentation = presentation;
    draw.texture = visual.texture;
    draw.order = {.layer = layer, .order = 0, .stableOrder = order++};
    state.draws.push_back(draw);
}

[[nodiscard]] std::uint32_t PlayerDirectionRow(const NightfallSurvivorsGame& game) noexcept
{
    const auto facing = game.Facing();
    if (std::abs(facing.x) > std::abs(facing.y)) return facing.x < 0.0F ? 1U : 2U;
    return facing.y > 0.0F ? 3U : 0U;
}

void AddHud(
    PresentationState& state,
    const trace2d::scene::Vector2 cameraCenter,
    std::uint64_t& order)
{
    const auto& game = *state.game;
    const float left = cameraCenter.x - 7.25F;
    const float top = cameraCenter.y + 4.02F;
    const float bottom = cameraCenter.y - 4.05F;

    const float healthRatio = game.MaximumHealth() == 0U ? 0.0F :
        static_cast<float>(game.Health()) / static_cast<float>(game.MaximumHealth());
    AddVisual(state, state.white, 0U, {left + 1.35F, top}, {2.7F, 0.18F}, 0.0F,
        {0.10F, 0.12F, 0.16F, 1.0F}, 0.92F, trace2d::render::SpriteBlendMode::Normal, 30, order);
    if (healthRatio > 0.0F)
        AddVisual(state, state.white, 0U, {left + 2.7F * healthRatio * 0.5F, top},
            {2.7F * healthRatio, 0.12F}, 0.0F,
            {0.95F, 0.22F, 0.30F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);

    const float xpRatio = game.ExperienceToNextLevel() == 0U ? 0.0F :
        static_cast<float>(game.Experience()) / static_cast<float>(game.ExperienceToNextLevel());
    AddVisual(state, state.white, 0U, {cameraCenter.x, bottom}, {7.45F, 0.11F}, 0.0F,
        {0.07F, 0.08F, 0.12F, 1.0F}, 0.94F, trace2d::render::SpriteBlendMode::Normal, 30, order);
    if (xpRatio > 0.0F)
        AddVisual(state, state.white, 0U, {cameraCenter.x - 7.45F + 7.45F * xpRatio, bottom},
            {7.45F * xpRatio, 0.075F}, 0.0F,
            {0.35F, 0.52F, 1.0F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);

    // Upgrade pips: blue = rapid, red = might, gold = orbit.
    for (std::uint32_t index = 0U; index < game.RapidLevel(); ++index)
        AddVisual(state, state.white, 0U, {left + 0.16F + index * 0.20F, top - 0.34F}, {0.07F, 0.07F}, 0.0F,
            {0.25F, 0.55F, 1.0F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);
    for (std::uint32_t index = 0U; index < game.MightLevel(); ++index)
        AddVisual(state, state.white, 0U, {left + 0.16F + index * 0.20F, top - 0.55F}, {0.07F, 0.07F}, 0.0F,
            {1.0F, 0.30F, 0.30F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);
    for (std::uint32_t index = 0U; index < game.OrbitLevel(); ++index)
        AddVisual(state, state.white, 0U, {left + 0.16F + index * 0.20F, top - 0.76F}, {0.07F, 0.07F}, 0.0F,
            {1.0F, 0.72F, 0.20F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);
}

void AddLevelUpOverlay(
    PresentationState& state,
    const trace2d::scene::Vector2 cameraCenter,
    std::uint64_t& order)
{
    AddVisual(state, state.white, 0U, cameraCenter, {7.7F, 4.15F}, 0.0F,
        {0.025F, 0.035F, 0.065F, 1.0F}, 0.78F, trace2d::render::SpriteBlendMode::Normal, 50, order);

    constexpr std::array<float, 3U> X{-3.0F, 0.0F, 3.0F};
    constexpr std::array<Color, 3U> Colors{{
        {0.20F, 0.50F, 1.0F, 1.0F},
        {1.0F, 0.25F, 0.28F, 1.0F},
        {1.0F, 0.72F, 0.18F, 1.0F},
    }};
    for (std::size_t index = 0U; index < X.size(); ++index)
    {
        AddVisual(state, state.white, 0U, {cameraCenter.x + X[index], cameraCenter.y}, {2.15F, 2.25F}, 0.0F,
            Colors[index], 0.23F, trace2d::render::SpriteBlendMode::Normal, 51, order);
        AddVisual(state, state.white, 0U, {cameraCenter.x + X[index], cameraCenter.y}, {1.93F, 2.03F}, 0.0F,
            {0.045F, 0.055F, 0.085F, 1.0F}, 0.96F, trace2d::render::SpriteBlendMode::Normal, 52, order);
        AddVisual(state, state.particle, 0U, {cameraCenter.x + X[index], cameraCenter.y + 0.50F}, {0.62F, 0.62F}, 0.0F,
            Colors[index], 1.0F, trace2d::render::SpriteBlendMode::Additive, 53, order);
    }
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr || state->game == nullptr)
        throw std::runtime_error{"Nightfall Survivors presentation state is unavailable."};
    const trace2d::scene::Entity* const player = context.Scene().TryGet(state->game->Player());
    if (player == nullptr) throw std::runtime_error{"Nightfall Survivors presentation lost the player."};

    AdvanceWalkAnimation(*state);
    state->draws.clear();
    std::uint64_t order = 0U;
    const trace2d::scene::Vector2 playerPosition = player->LocalTransform().position;

    // Infinite-looking repeated dungeon floor around the moving camera.
    const int tileOriginX = static_cast<int>(std::floor(playerPosition.x)) - 9;
    const int tileOriginY = static_cast<int>(std::floor(playerPosition.y)) - 6;
    for (int row = 0; row < 13; ++row)
    {
        for (int column = 0; column < 20; ++column)
        {
            const int tileX = tileOriginX + column;
            const int tileY = tileOriginY + row;
            const bool accent = ((tileX * 17 + tileY * 31) & 15) == 0;
            AddVisual(*state, accent ? state->floorAlt : state->floor, 0U,
                {static_cast<float>(tileX) + 0.5F, static_cast<float>(tileY) + 0.5F},
                {1.0F, 1.0F}, 0.0F, {1.0F, 1.0F, 1.0F, 1.0F}, 1.0F,
                trace2d::render::SpriteBlendMode::Normal, -20, order);
        }
    }

    const float animationPhase = static_cast<float>(state->game->FrameCounter() % 120U) * 0.0523598776F;
    for (const auto& gem : state->game->Gems())
    {
        if (!gem.active) continue;
        const float pulse = 0.31F + std::sin(animationPhase + gem.stableId * 0.31F) * 0.04F;
        AddVisual(*state, state->particle, 0U, gem.position, {pulse, pulse}, 0.0F,
            {0.42F, 0.52F, 1.0F, 1.0F}, 1.0F,
            trace2d::render::SpriteBlendMode::Additive, 0, order);
    }

    for (const auto& enemy : state->game->Enemies())
    {
        if (!enemy.active) continue;
        const float bob = std::sin(animationPhase * 1.6F + enemy.stableId * 0.37F) * 0.05F;
        const Visual* visual = &state->ghoul;
        Color tint{0.78F, 0.94F, 0.78F, 1.0F};
        float scale = 0.90F;
        if (enemy.kind == NightfallSurvivorsGame::EnemyKind::Brute)
        {
            visual = &state->brute;
            tint = {1.0F, 0.63F, 0.48F, 1.0F};
            scale = 1.20F;
        }
        else if (enemy.kind == NightfallSurvivorsGame::EnemyKind::Wisp)
        {
            tint = {0.62F, 0.55F, 1.0F, 0.92F};
            scale = 0.76F;
        }
        if (enemy.hitFlashFrames > 0U) tint = {1.0F, 1.0F, 0.72F, 1.0F};
        AddVisual(*state, *visual, 0U, {enemy.position.x, enemy.position.y + bob}, {scale, scale}, 0.0F,
            tint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 4, order);
    }

    for (const auto& projectile : state->game->Projectiles())
    {
        if (!projectile.active) continue;
        const float rotation = std::atan2(projectile.velocity.y, projectile.velocity.x);
        AddVisual(*state, state->particle, 0U, projectile.position, {0.28F, 0.16F}, rotation,
            {0.28F, 0.88F, 1.0F, 1.0F}, 1.0F,
            trace2d::render::SpriteBlendMode::Additive, 8, order);
    }

    for (const auto& effect : state->game->Effects())
    {
        if (!effect.active || effect.lifetimeSeconds <= 0.0F) continue;
        const float t = std::clamp(effect.ageSeconds / effect.lifetimeSeconds, 0.0F, 1.0F);
        const float scale = effect.startScale + (effect.endScale - effect.startScale) * t;
        Color tint{1.0F, 0.78F, 0.30F, 1.0F};
        if (effect.kind == NightfallSurvivorsGame::EffectKind::Death) tint = {1.0F, 0.34F, 0.24F, 1.0F};
        else if (effect.kind == NightfallSurvivorsGame::EffectKind::LevelUp) tint = {0.40F, 0.72F, 1.0F, 1.0F};
        else if (effect.kind == NightfallSurvivorsGame::EffectKind::PlayerHurt) tint = {1.0F, 0.18F, 0.28F, 1.0F};
        const float opacity = 1.0F - t;
        AddVisual(*state, state->particle, 0U, effect.position, {scale, scale}, t * Pi,
            tint, opacity, trace2d::render::SpriteBlendMode::Additive, 10, order);
        if (effect.kind == NightfallSurvivorsGame::EffectKind::Death)
        {
            for (std::uint32_t burst = 0U; burst < 4U; ++burst)
            {
                const float angle = burst * (Pi * 0.5F) + effect.stableId * 0.19F;
                const float distance = t * 0.55F;
                AddVisual(*state, state->particle, 0U,
                    {effect.position.x + std::cos(angle) * distance, effect.position.y + std::sin(angle) * distance},
                    {scale * 0.38F, scale * 0.38F}, -angle, tint, opacity,
                    trace2d::render::SpriteBlendMode::Additive, 10, order);
            }
        }
    }

    // Orbit weapon positions mirror the fixed-step gameplay formula.
    const std::uint32_t orbitCount = std::min<std::uint32_t>(state->game->OrbitLevel(), 4U);
    if (orbitCount > 0U)
    {
        const float orbitRadius = 1.25F + static_cast<float>(orbitCount) * 0.08F;
        const float baseAngle = state->game->ElapsedSeconds() * 3.4F;
        for (std::uint32_t blade = 0U; blade < orbitCount; ++blade)
        {
            const float angle = baseAngle + 2.0F * Pi * static_cast<float>(blade) / static_cast<float>(orbitCount);
            AddVisual(*state, state->particle, 0U,
                {playerPosition.x + std::cos(angle) * orbitRadius, playerPosition.y + std::sin(angle) * orbitRadius},
                {0.34F, 0.18F}, angle, {1.0F, 0.74F, 0.22F, 1.0F}, 1.0F,
                trace2d::render::SpriteBlendMode::Additive, 12, order);
        }
    }

    const std::uint32_t row = PlayerDirectionRow(*state->game);
    std::uint32_t heroRegion = row * 8U;
    if (state->game->MoveIntent().x != 0.0F || state->game->MoveIntent().y != 0.0F)
    {
        if (!state->walkAnimators[row].TryGetCurrentRegionIndex(heroRegion))
            throw std::runtime_error{"Nightfall Survivors could not resolve the current walk frame."};
    }
    Color heroTint{1.0F, 1.0F, 1.0F, 1.0F};
    if (state->game->PlayerFlashFrames() > 0U && (state->game->FrameCounter() & 1U) != 0U)
        heroTint = {1.0F, 0.42F, 0.42F, 1.0F};
    AddVisual(*state, state->hero, heroRegion, playerPosition, {1.15F, 1.15F}, 0.0F,
        heroTint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 15, order);

    AddHud(*state, playerPosition, order);
    if (state->game->CurrentState() == NightfallSurvivorsGame::State::LevelUp)
        AddLevelUpOverlay(*state, playerPosition, order);
    else if (state->game->CurrentState() == NightfallSurvivorsGame::State::Lost ||
             state->game->CurrentState() == NightfallSurvivorsGame::State::Won)
    {
        const Color border = state->game->CurrentState() == NightfallSurvivorsGame::State::Won
            ? Color{0.24F, 1.0F, 0.48F, 1.0F}
            : Color{1.0F, 0.18F, 0.24F, 1.0F};
        AddVisual(*state, state->white, 0U, {playerPosition.x, playerPosition.y + 4.22F}, {7.7F, 0.10F}, 0.0F,
            border, 1.0F, trace2d::render::SpriteBlendMode::Normal, 60, order);
        AddVisual(*state, state->white, 0U, {playerPosition.x, playerPosition.y - 4.22F}, {7.7F, 0.10F}, 0.0F,
            border, 1.0F, trace2d::render::SpriteBlendMode::Normal, 60, order);
    }

    trace2d::render::OrthographicCamera camera{};
    const float shake = state->game->ScreenShakeFrames() > 0U
        ? (((state->game->FrameCounter() & 1U) != 0U) ? 0.08F : -0.08F)
        : 0.0F;
    camera.center = {playerPosition.x + shake, playerPosition.y - shake * 0.5F};
    camera.verticalSize = NightfallSurvivorsGame::CameraVerticalSize;
    state->renderer->RenderFrame(
        camera,
        std::span<const trace2d::render::SpritePresentationRenderData>{state->draws.data(), state->draws.size()});
}

void ReleaseVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    Visual& visual) noexcept
{
    if (!visual.texture.Untyped().domain == trace2d::assets::ResourceTypeDomain::Texture) return;
    renderer.DestroyTexture(visual.texture);
    static_cast<void>(resources.Unload(visual.texture.Untyped()));
}

void RequireOutput(const trace2d::audio::AudioOutputResult2D result, const char* const message)
{
    if (result != trace2d::audio::AudioOutputResult2D::Success) throw std::runtime_error{message};
}
} // namespace

int main()
{
    using namespace std::chrono_literals;
    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(NightfallSurvivorsGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(NightfallSurvivorsGame::CanvasHeight);
        platformConfig.windowTitle =
            "Trace2D G1 - Nightfall Survivors | WASD move | auto-fire | LEVEL UP: Q rapid / E might / F orbit | R restart | Esc quit";
        trace2d::platform::Platform platform{platformConfig};

        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.018F, .green = 0.020F, .blue = 0.030F, .alpha = 1.0F};
        trace2d::render::Renderer renderer{rendererConfig, platform};

        RegistryTypes types{};
        NightfallSurvivorsGame game{types.audio, TRACE2D_G1_RUNTIME_DIR};
        trace2d::application::Application application{game, types.registry, MakeConfig()};
        application.Start();

        trace2d::assets::TextureAssetCache textureCache{TRACE2D_G1_RUNTIME_DIR};
        PresentationState presentation{};
        presentation.renderer = &renderer;
        presentation.game = &game;
        presentation.floor = LoadVisual(textureCache, game.Resources(), renderer, "textures/floor.png", 1U, 1U, 16.0F);
        presentation.floorAlt = LoadVisual(textureCache, game.Resources(), renderer, "textures/floor-alt.png", 1U, 1U, 16.0F);
        presentation.hero = LoadVisual(textureCache, game.Resources(), renderer, "textures/warden-walk.png", 8U, 4U, 32.0F);
        presentation.ghoul = LoadVisual(textureCache, game.Resources(), renderer, "textures/ghoul.png", 1U, 1U, 16.0F);
        presentation.brute = LoadVisual(textureCache, game.Resources(), renderer, "textures/brute.png", 1U, 1U, 16.0F);
        presentation.particle = LoadVisual(textureCache, game.Resources(), renderer, "textures/particle.png", 1U, 1U, 96.0F);
        presentation.white = CreateWhiteVisual(game.Resources(), renderer);
        presentation.draws.reserve(1800U);
        PrepareWalkAnimation(presentation);
        application.SetPresentationCallback(&Present, &presentation);

        trace2d::audio::AudioOutputConfig2D audioConfig{};
        audioConfig.voiceCapacity = 32U;
        audioConfig.preloadCacheCapacity = 5U;
        audioConfig.preloadPcmByteBudget = 16U * 1024U * 1024U;
        audioConfig.refillBufferByteBudget = 2U * 1024U * 1024U;
        trace2d::audio::AudioOutput2D audioOutput{game.Resources(), audioConfig};
        RequireOutput(audioOutput.Start(), "Nightfall Survivors could not start physical audio output.");

        bool quitRequested = false;
        Clock::time_point previous = Clock::now();
        while (!quitRequested)
        {
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                {
                    quitRequested = true;
                }
                else if (event.type == trace2d::platform::PlatformEventType::Input)
                {
                    if (event.input.control == trace2d::input::InputControl::Escape &&
                        event.input.type == trace2d::input::InputEventType::Press)
                        quitRequested = true;
                    else
                        application.ApplyInput(event.input);
                }
            }
            if (quitRequested) break;

            const Clock::time_point now = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
            previous = now;
            elapsed = std::min(elapsed, std::chrono::duration_cast<std::chrono::nanoseconds>(250ms));
            static_cast<void>(application.AdvanceElapsed(elapsed));

            const auto sync = audioOutput.Sync(game.Audio(), game.Audio().Events());
            RequireOutput(sync.result, "Nightfall Survivors physical audio sync failed.");
            game.Audio().ClearEvents();
            const auto deviceEvents = audioOutput.PollDeviceEvents();
            RequireOutput(deviceEvents.result, "Nightfall Survivors audio device polling failed.");
            if (deviceEvents.recoveryRequested)
                RequireOutput(audioOutput.Recover(game.Audio()), "Nightfall Survivors audio recovery failed.");
            RequireOutput(audioOutput.Pump().result, "Nightfall Survivors audio pump failed.");

            static_cast<void>(application.Present());
            std::this_thread::sleep_for(1ms);
        }

        const auto audioMetrics = audioOutput.Metrics();
        const auto renderMetrics = renderer.Metrics();
        std::cout
            << "Nightfall Survivors owner metrics: level=" << game.Level()
            << ", kills=" << game.KillCount()
            << ", elapsed=" << game.ElapsedSeconds()
            << ", draw_calls=" << renderMetrics.drawCalls
            << ", audio_streams=" << audioMetrics.streamCreateCount
            << '\n';

        audioOutput.Stop();
        application.Stop();
        ReleaseVisual(game.Resources(), renderer, presentation.white);
        ReleaseVisual(game.Resources(), renderer, presentation.particle);
        ReleaseVisual(game.Resources(), renderer, presentation.brute);
        ReleaseVisual(game.Resources(), renderer, presentation.ghoul);
        ReleaseVisual(game.Resources(), renderer, presentation.hero);
        ReleaseVisual(game.Resources(), renderer, presentation.floorAlt);
        ReleaseVisual(game.Resources(), renderer, presentation.floor);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Nightfall Survivors failed: " << error.what() << '\n';
        return 1;
    }
}
