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
#include <iostream>
#include <memory>
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
    std::shared_ptr<trace2d::assets::SpriteAsset> asset{};
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

[[nodiscard]] Visual MakeVisual(
    trace2d::assets::TextureAssetCache& cache,
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const std::string& reference,
    const std::uint32_t columns,
    const std::uint32_t rows,
    const float pixelsPerUnit)
{
    const auto loaded = cache.Load(reference);
    if (!loaded.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not load asset: " + reference};
    if (columns == 0U || rows == 0U || loaded.asset->width % columns != 0U || loaded.asset->height % rows != 0U)
        throw std::runtime_error{"Nightfall Survivors sprite grid does not divide source texture."};

    trace2d::assets::TextureResource canonical{};
    canonical.width = loaded.asset->width;
    canonical.height = loaded.asset->height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Srgb;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors owner-playable asset";
    canonical.canonicalRgba8 = loaded.asset->rgba8;
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not publish texture."};

    trace2d::render::Rgba8TextureData gpuData{};
    gpuData.width = loaded.asset->width;
    gpuData.height = loaded.asset->height;
    gpuData.pixels = std::span<const std::uint8_t>{loaded.asset->rgba8.data(), loaded.asset->rgba8.size()};

    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle, gpuData, trace2d::render::SpriteTextureEncoding::Srgb);
    visual.pixelsPerUnit = pixelsPerUnit;
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = reference + ".sprite";
    visual.asset->sampling = trace2d::assets::SpriteSampling::Nearest;
    visual.asset->pages.push_back({
        .id = "page",
        .textureReference = reference,
        .size = {loaded.asset->width, loaded.asset->height},
        .colorSpace = trace2d::assets::SpriteColorSpace::Srgb,
    });

    const std::uint32_t cellWidth = loaded.asset->width / columns;
    const std::uint32_t cellHeight = loaded.asset->height / rows;
    visual.asset->regions.reserve(static_cast<std::size_t>(columns) * rows);
    for (std::uint32_t row = 0U; row < rows; ++row)
    {
        for (std::uint32_t column = 0U; column < columns; ++column)
        {
            trace2d::assets::SpriteRegion region{};
            region.id = "region-" + std::to_string(row * columns + column);
            region.pageId = "page";
            region.sourceSize = {cellWidth, cellHeight};
            region.trimSize = {cellWidth, cellHeight};
            region.packedRect = {column * cellWidth, row * cellHeight, cellWidth, cellHeight};
            region.pivot = {
                static_cast<std::int64_t>(cellWidth),
                static_cast<std::int64_t>(cellHeight),
                2,
            };
            visual.asset->regions.push_back(std::move(region));
        }
    }

    visual.selections.resize(visual.asset->regions.size());
    for (std::size_t index = 0U; index < visual.selections.size(); ++index)
    {
        if (!trace2d::render::ResolveSpriteRegionByIndices(
                visual.asset.get(), 0U, index, visual.selections[index]).Succeeded())
            throw std::runtime_error{"Nightfall Survivors could not resolve sprite region."};
    }
    return visual;
}

[[nodiscard]] Visual MakeWhiteVisual(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer)
{
    constexpr std::array<std::uint8_t, 4U> White{255U, 255U, 255U, 255U};
    constexpr const char* Reference = "generated/nightfall-white.rgba8";
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Nightfall Survivors HUD primitive";
    canonical.canonicalRgba8.assign(White.begin(), White.end());
    const auto published = resources.PublishTexture(Reference, std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"Nightfall Survivors could not publish HUD texture."};

    trace2d::render::Rgba8TextureData data{1U, 1U, std::span<const std::uint8_t>{White}};
    Visual visual{};
    visual.texture = renderer.CreateSpriteTextureRgba8(
        published.handle, data, trace2d::render::SpriteTextureEncoding::Linear);
    visual.asset = std::make_shared<trace2d::assets::SpriteAsset>();
    visual.asset->id = "nightfall-white.sprite";
    visual.asset->sampling = trace2d::assets::SpriteSampling::Nearest;
    visual.asset->pages.push_back({
        .id = "page",
        .textureReference = Reference,
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
    visual.asset->regions.push_back(std::move(region));
    visual.selections.resize(1U);
    if (!trace2d::render::ResolveSpriteRegionByIndices(
            visual.asset.get(), 0U, 0U, visual.selections[0]).Succeeded())
        throw std::runtime_error{"Nightfall Survivors could not resolve HUD visual."};
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
                state.hero.asset.get(),
                static_cast<std::uint32_t>(state.hero.asset->regions.size()),
                frames,
                state.walkClips[row]).Succeeded())
            throw std::runtime_error{"Nightfall Survivors could not prepare walk animation."};

        trace2d::runtime::SpriteAnimator2DState animatorState{};
        if (!trace2d::runtime::MakeSpriteAnimator2DState(
                state.walkClips[row], {},
                trace2d::runtime::SpriteAnimationPlaybackState::Playing,
                trace2d::runtime::SpriteAnimationLoopMode::Loop,
                trace2d::runtime::SpriteAnimationDirection::Forward,
                false, {1U, 1U}, animatorState).Succeeded() ||
            !state.walkAnimators[row].RestoreState(animatorState).Succeeded())
            throw std::runtime_error{"Nightfall Survivors could not initialize walk animator."};
    }
}

void AdvanceWalkAnimation(PresentationState& state)
{
    const std::uint64_t frame = state.game->FrameCounter();
    if (frame <= state.lastAnimationFrame) return;
    const auto delta = std::chrono::nanoseconds{
        static_cast<std::int64_t>(frame - state.lastAnimationFrame) * 16'666'667LL};
    state.lastAnimationFrame = frame;
    std::array<trace2d::runtime::SpriteAnimationEmission2D, 32U> emissions{};
    for (auto& animator : state.walkAnimators)
    {
        if (!animator.Advance(delta, emissions).Succeeded())
            throw std::runtime_error{"Nightfall Survivors walk animation advance failed."};
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
    if (regionIndex >= visual.selections.size()) throw std::runtime_error{"Nightfall Survivors region overflow."};
    trace2d::scene::SpritePose2D pose{};
    pose.transform.position = position;
    pose.transform.rotationRadians = rotation;
    pose.transform.scale = scale;
    trace2d::render::SpriteAppearance2D appearance{};
    appearance.tint = tint;
    appearance.opacity = opacity;
    appearance.sampling = trace2d::render::SpriteAppearanceSampling::Nearest;
    appearance.blend = blend;

    trace2d::render::SpritePresentationRenderData draw{};
    if (!trace2d::render::BuildSpritePresentation2D(
            visual.selections[regionIndex], pose, visual.pixelsPerUnit, appearance, draw.presentation).Succeeded())
        throw std::runtime_error{"Nightfall Survivors sprite presentation failed."};
    draw.texture = visual.texture;
    draw.order.layer = layer;
    draw.order.stableOrder = order++;
    state.draws.push_back(draw);
}

[[nodiscard]] std::uint32_t DirectionRow(const NightfallSurvivorsGame& game) noexcept
{
    // arikel's rpg_sprite_walk.png row order is down, up, left, right.
    const auto facing = game.Facing();
    if (std::abs(facing.x) > std::abs(facing.y)) return facing.x < 0.0F ? 2U : 3U;
    return facing.y > 0.0F ? 1U : 0U;
}

void AddHud(PresentationState& state, const trace2d::scene::Vector2 center, std::uint64_t& order)
{
    const auto& game = *state.game;
    const float healthRatio = static_cast<float>(game.Health()) / static_cast<float>(game.MaximumHealth());
    const float xpRatio = static_cast<float>(game.Experience()) / static_cast<float>(game.ExperienceToNextLevel());
    const float left = center.x - 7.35F;
    const float top = center.y + 4.05F;
    const float bottom = center.y - 4.08F;

    AddVisual(state, state.white, 0U, {left + 1.5F, top}, {3.0F, 0.18F}, 0.0F,
        {0.10F, 0.11F, 0.14F, 1.0F}, 0.95F, trace2d::render::SpriteBlendMode::Normal, 30, order);
    AddVisual(state, state.white, 0U, {left + 1.5F * healthRatio, top}, {3.0F * healthRatio, 0.12F}, 0.0F,
        {0.95F, 0.20F, 0.28F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);

    constexpr float XpWidth = 14.7F;
    AddVisual(state, state.white, 0U, {center.x, bottom}, {XpWidth, 0.12F}, 0.0F,
        {0.06F, 0.07F, 0.11F, 1.0F}, 0.95F, trace2d::render::SpriteBlendMode::Normal, 30, order);
    if (xpRatio > 0.0F)
        AddVisual(state, state.white, 0U, {center.x - XpWidth * 0.5F + XpWidth * xpRatio * 0.5F, bottom},
            {XpWidth * xpRatio, 0.08F}, 0.0F,
            {0.35F, 0.52F, 1.0F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, 31, order);
}

void AddLevelUpOverlay(PresentationState& state, const trace2d::scene::Vector2 center, std::uint64_t& order)
{
    AddVisual(state, state.white, 0U, center, {15.5F, 8.35F}, 0.0F,
        {0.02F, 0.03F, 0.06F, 1.0F}, 0.80F, trace2d::render::SpriteBlendMode::Normal, 50, order);
    constexpr std::array<float, 3U> X{-3.0F, 0.0F, 3.0F};
    constexpr std::array<Color, 3U> Colors{{
        {0.20F, 0.50F, 1.0F, 1.0F},
        {1.0F, 0.25F, 0.28F, 1.0F},
        {1.0F, 0.72F, 0.18F, 1.0F},
    }};
    for (std::size_t index = 0U; index < X.size(); ++index)
    {
        AddVisual(state, state.white, 0U, {center.x + X[index], center.y}, {2.2F, 3.6F}, 0.0F,
            Colors[index], 0.28F, trace2d::render::SpriteBlendMode::Normal, 51, order);
        AddVisual(state, state.particle, 0U, {center.x + X[index], center.y + 0.35F}, {0.75F, 0.75F}, 0.0F,
            Colors[index], 1.0F, trace2d::render::SpriteBlendMode::Additive, 52, order);
    }
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr || state->game == nullptr)
        throw std::runtime_error{"Nightfall Survivors presentation state unavailable."};
    const trace2d::scene::Entity* const player = context.Scene().TryGet(state->game->Player());
    if (player == nullptr) throw std::runtime_error{"Nightfall Survivors lost presentation player."};

    AdvanceWalkAnimation(*state);
    state->draws.clear();
    std::uint64_t order = 0U;
    const auto playerPosition = player->LocalTransform().position;

    const int originX = static_cast<int>(std::floor(playerPosition.x)) - 9;
    const int originY = static_cast<int>(std::floor(playerPosition.y)) - 6;
    for (int row = 0; row < 13; ++row)
    {
        for (int column = 0; column < 20; ++column)
        {
            const int x = originX + column;
            const int y = originY + row;
            const bool accent = ((x * 17 + y * 31) & 15) == 0;
            AddVisual(*state, accent ? state->floorAlt : state->floor, 0U,
                {static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F}, {1.0F, 1.0F}, 0.0F,
                {1.0F, 1.0F, 1.0F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Normal, -20, order);
        }
    }

    const float phase = static_cast<float>(state->game->FrameCounter() % 120U) * 0.0523598776F;
    for (const auto& gem : state->game->Gems())
    {
        if (!gem.active) continue;
        const float pulse = 0.30F + std::sin(phase + gem.stableId * 0.31F) * 0.04F;
        AddVisual(*state, state->particle, 0U, gem.position, {pulse, pulse}, 0.0F,
            {0.40F, 0.55F, 1.0F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Additive, 0, order);
    }

    for (const auto& enemy : state->game->Enemies())
    {
        if (!enemy.active) continue;
        const Visual& visual = enemy.kind == NightfallSurvivorsGame::EnemyKind::Brute ? state->brute : state->ghoul;
        float scale = enemy.kind == NightfallSurvivorsGame::EnemyKind::Brute ? 1.20F : 0.92F;
        Color tint = enemy.kind == NightfallSurvivorsGame::EnemyKind::Wisp
            ? Color{0.62F, 0.55F, 1.0F, 0.92F}
            : Color{1.0F, 1.0F, 1.0F, 1.0F};
        if (enemy.hitFlashFrames > 0U) tint = {1.0F, 0.92F, 0.48F, 1.0F};
        const float bob = std::sin(phase * 1.5F + enemy.stableId * 0.37F) * 0.04F;
        AddVisual(*state, visual, 0U, {enemy.position.x, enemy.position.y + bob}, {scale, scale}, 0.0F,
            tint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 4, order);
    }

    for (const auto& projectile : state->game->Projectiles())
    {
        if (!projectile.active) continue;
        AddVisual(*state, state->particle, 0U, projectile.position, {0.28F, 0.16F},
            std::atan2(projectile.velocity.y, projectile.velocity.x),
            {0.25F, 0.88F, 1.0F, 1.0F}, 1.0F, trace2d::render::SpriteBlendMode::Additive, 8, order);
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
        AddVisual(*state, state->particle, 0U, effect.position, {scale, scale}, t * Pi,
            tint, 1.0F - t, trace2d::render::SpriteBlendMode::Additive, 10, order);
        if (effect.kind == NightfallSurvivorsGame::EffectKind::Death)
        {
            for (std::uint32_t burst = 0U; burst < 4U; ++burst)
            {
                const float angle = burst * Pi * 0.5F + effect.stableId * 0.19F;
                const float distance = t * 0.55F;
                AddVisual(*state, state->particle, 0U,
                    {effect.position.x + std::cos(angle) * distance, effect.position.y + std::sin(angle) * distance},
                    {scale * 0.38F, scale * 0.38F}, angle, tint, 1.0F - t,
                    trace2d::render::SpriteBlendMode::Additive, 10, order);
            }
        }
    }

    const std::uint32_t orbitCount = std::min<std::uint32_t>(state->game->OrbitLevel(), 4U);
    for (std::uint32_t blade = 0U; blade < orbitCount; ++blade)
    {
        const float angle = state->game->ElapsedSeconds() * 3.4F + 2.0F * Pi * blade / orbitCount;
        const float radius = 1.25F + orbitCount * 0.08F;
        AddVisual(*state, state->particle, 0U,
            {playerPosition.x + std::cos(angle) * radius, playerPosition.y + std::sin(angle) * radius},
            {0.34F, 0.18F}, angle, {1.0F, 0.74F, 0.22F, 1.0F}, 1.0F,
            trace2d::render::SpriteBlendMode::Additive, 12, order);
    }

    const std::uint32_t row = DirectionRow(*state->game);
    std::uint32_t heroRegion = row * 8U;
    if (state->game->MoveIntent().x != 0.0F || state->game->MoveIntent().y != 0.0F)
    {
        if (!state->walkAnimators[row].TryGetCurrentRegionIndex(heroRegion))
            throw std::runtime_error{"Nightfall Survivors could not resolve walk frame."};
    }
    Color heroTint{1.0F, 1.0F, 1.0F, 1.0F};
    if (state->game->PlayerFlashFrames() > 0U && (state->game->FrameCounter() & 1U) != 0U)
        heroTint = {1.0F, 0.42F, 0.42F, 1.0F};
    AddVisual(*state, state->hero, heroRegion, playerPosition, {1.15F, 1.15F}, 0.0F,
        heroTint, 1.0F, trace2d::render::SpriteBlendMode::Normal, 15, order);

    AddHud(*state, playerPosition, order);
    if (state->game->CurrentState() == NightfallSurvivorsGame::State::LevelUp)
        AddLevelUpOverlay(*state, playerPosition, order);

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

void ReleaseVisual(trace2d::assets::ResourceRegistry& resources, trace2d::render::Renderer& renderer, Visual& visual) noexcept
{
    renderer.DestroyTexture(visual.texture);
    static_cast<void>(resources.Unload(visual.texture.Untyped()));
    visual.selections.clear();
    visual.asset.reset();
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
            "Trace2D - Nightfall Survivors | WASD | auto-fire | level-up: Q Rapid / E Might / F Orbit | R restart";
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
        presentation.floor = MakeVisual(textureCache, game.Resources(), renderer, "textures/floor.png", 1U, 1U, 16.0F);
        presentation.floorAlt = MakeVisual(textureCache, game.Resources(), renderer, "textures/floor-alt.png", 1U, 1U, 16.0F);
        presentation.hero = MakeVisual(textureCache, game.Resources(), renderer, "textures/warden-walk.png", 8U, 4U, 32.0F);
        presentation.ghoul = MakeVisual(textureCache, game.Resources(), renderer, "textures/ghoul.png", 1U, 1U, 16.0F);
        presentation.brute = MakeVisual(textureCache, game.Resources(), renderer, "textures/brute.png", 1U, 1U, 16.0F);
        presentation.particle = MakeVisual(textureCache, game.Resources(), renderer, "textures/particle.png", 1U, 1U, 96.0F);
        presentation.white = MakeWhiteVisual(game.Resources(), renderer);
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

        bool quit = false;
        auto previous = Clock::now();
        while (!quit)
        {
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                    quit = true;
                else if (event.type == trace2d::platform::PlatformEventType::Input)
                {
                    if (event.input.control == trace2d::input::InputControl::Escape &&
                        event.input.type == trace2d::input::InputEventType::Press)
                        quit = true;
                    else
                        application.ApplyInput(event.input);
                }
            }
            if (quit) break;

            const auto now = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
            previous = now;
            elapsed = std::min(elapsed, std::chrono::duration_cast<std::chrono::nanoseconds>(250ms));
            static_cast<void>(application.AdvanceElapsed(elapsed));

            const auto sync = audioOutput.Sync(game.Audio(), game.Audio().Events());
            RequireOutput(sync.result, "Nightfall Survivors audio sync failed.");
            game.Audio().ClearEvents();
            const auto deviceEvents = audioOutput.PollDeviceEvents();
            RequireOutput(deviceEvents.result, "Nightfall Survivors audio device polling failed.");
            if (deviceEvents.recoveryRequested)
                RequireOutput(audioOutput.Recover(game.Audio()), "Nightfall Survivors audio recovery failed.");
            RequireOutput(audioOutput.Pump().result, "Nightfall Survivors audio pump failed.");

            static_cast<void>(application.Present());
            std::this_thread::sleep_for(1ms);
        }

        const auto renderMetrics = renderer.Metrics();
        std::cout << "Nightfall Survivors: level=" << game.Level()
                  << ", kills=" << game.KillCount()
                  << ", elapsed=" << game.ElapsedSeconds()
                  << ", draw_calls=" << renderMetrics.drawCalls << '\n';

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
