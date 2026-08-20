#include "CombatGame.hpp"
#include "P2GeneratedAssets.hpp"

#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/ui/Ui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef TRACE2D_P2_RUNTIME_DIR
#define TRACE2D_P2_RUNTIME_DIR "."
#endif

namespace
{
using trace2d::examples::CombatGame;
using TextureHandle = trace2d::render::TextureHandle;
using Color = std::array<std::uint8_t, 4U>;
using SpriteBuffer = std::array<trace2d::render::SpriteRenderData, 32U>;

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

struct PresentationState final
{
    trace2d::render::Renderer* renderer{nullptr};
    CombatGame* game{nullptr};
    TextureHandle arena{};
    TextureHandle wall{};
    TextureHandle player{};
    TextureHandle enemy{};
    TextureHandle enemyFlash{};
    TextureHandle attack{};
    TextureHandle healthFull{};
    TextureHandle healthEmpty{};
    trace2d::scene::Vector2 previousPlayerPosition{};
    trace2d::scene::Vector2 currentPlayerPosition{};
    trace2d::scene::Vector2 previousEnemyPosition{};
    trace2d::scene::Vector2 currentEnemyPosition{};
    std::uint64_t sampledSimulationFrame{0U};
    bool interpolationInitialized{false};
    bool captureRequested{false};
    std::uint64_t captureFrame{1U};
};

[[nodiscard]] trace2d::application::ApplicationConfig MakeConfig()
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

[[nodiscard]] TextureHandle CreateSolidTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    std::string reference,
    const Color color)
{
    trace2d::assets::TextureResource texture{};
    texture.width = 1U;
    texture.height = 1U;
    texture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    texture.canonicalRgba8.assign(color.begin(), color.end());
    const auto published = resources.PublishTexture(std::move(reference), std::move(texture));
    if (!published.Succeeded()) throw std::runtime_error{"P2 could not publish a presentation texture."};

    const std::array<std::uint8_t, 4U> pixel{color[0], color[1], color[2], color[3]};
    trace2d::render::Rgba8TextureData data{};
    data.width = 1U;
    data.height = 1U;
    data.pixels = std::span<const std::uint8_t>{pixel.data(), pixel.size()};
    return renderer.CreateTextureRgba8(published.handle, data);
}

void AddSprite(
    SpriteBuffer& sprites,
    std::size_t& count,
    const float x,
    const float y,
    const float halfWidth,
    const float halfHeight,
    const TextureHandle texture,
    const std::int32_t layer,
    std::uint64_t& order)
{
    if (count >= sprites.size()) throw std::runtime_error{"P2 presentation sprite capacity exceeded."};
    sprites[count++] = trace2d::render::SpriteRenderData{
        .center = {x, y},
        .halfExtents = {halfWidth, halfHeight},
        .texture = texture,
        .layer = layer,
        .stableOrder = order++,
    };
}

[[nodiscard]] trace2d::scene::Vector2 Lerp(
    const trace2d::scene::Vector2 from,
    const trace2d::scene::Vector2 to,
    const float alpha) noexcept
{
    return {
        from.x + (to.x - from.x) * alpha,
        from.y + (to.y - from.y) * alpha,
    };
}

[[nodiscard]] float DistanceSquared(
    const trace2d::scene::Vector2 left,
    const trace2d::scene::Vector2 right) noexcept
{
    const float x = right.x - left.x;
    const float y = right.y - left.y;
    return x * x + y * y;
}

void UpdateInterpolationHistory(
    PresentationState& state,
    const std::uint64_t simulationFrame,
    const trace2d::scene::Vector2 playerPosition,
    const trace2d::scene::Vector2 enemyPosition) noexcept
{
    if (!state.interpolationInitialized)
    {
        state.previousPlayerPosition = playerPosition;
        state.currentPlayerPosition = playerPosition;
        state.previousEnemyPosition = enemyPosition;
        state.currentEnemyPosition = enemyPosition;
        state.sampledSimulationFrame = simulationFrame;
        state.interpolationInitialized = true;
        return;
    }

    if (simulationFrame == state.sampledSimulationFrame) return;

    const bool contiguousFrame = simulationFrame == state.sampledSimulationFrame + 1U;
    constexpr float TeleportResetDistanceSquared = 1.0F;
    const bool playerTeleported =
        DistanceSquared(state.currentPlayerPosition, playerPosition) > TeleportResetDistanceSquared;
    const bool enemyTeleported =
        DistanceSquared(state.currentEnemyPosition, enemyPosition) > TeleportResetDistanceSquared;

    if (contiguousFrame && !playerTeleported)
    {
        state.previousPlayerPosition = state.currentPlayerPosition;
        state.currentPlayerPosition = playerPosition;
    }
    else
    {
        state.previousPlayerPosition = playerPosition;
        state.currentPlayerPosition = playerPosition;
    }

    if (contiguousFrame && !enemyTeleported)
    {
        state.previousEnemyPosition = state.currentEnemyPosition;
        state.currentEnemyPosition = enemyPosition;
    }
    else
    {
        state.previousEnemyPosition = enemyPosition;
        state.currentEnemyPosition = enemyPosition;
    }

    state.sampledSimulationFrame = simulationFrame;
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr || state->game == nullptr)
        throw std::runtime_error{"P2 presentation state is unavailable."};

    trace2d::physics::PhysicsBodyState2D player{};
    trace2d::physics::PhysicsBodyState2D enemy{};
    if (!state->game->TryPlayerBodyState(player) || !state->game->TryEnemyBodyState(enemy))
        throw std::runtime_error{"P2 presentation could not inspect gameplay body state."};

    const trace2d::runtime::RuntimeState runtimeState = context.Runtime().State();
    UpdateInterpolationHistory(*state, runtimeState.frame, player.position, enemy.position);

    const auto fixedTimestep = context.Runtime().Config().fixedTimestep;
    const float interpolationAlpha = fixedTimestep.count() > 0
        ? std::clamp(
              static_cast<float>(runtimeState.accumulatedWallTime.count()) /
                  static_cast<float>(fixedTimestep.count()),
              0.0F,
              1.0F)
        : 1.0F;
    const trace2d::scene::Vector2 presentedPlayerPosition =
        Lerp(state->previousPlayerPosition, state->currentPlayerPosition, interpolationAlpha);
    const trace2d::scene::Vector2 presentedEnemyPosition =
        Lerp(state->previousEnemyPosition, state->currentEnemyPosition, interpolationAlpha);

    const trace2d::ui::UiElement* const enemyHealth = context.Ui().Find("hud.enemy-health");
    if (enemyHealth == nullptr || !enemyHealth->progress.Active())
        throw std::runtime_error{"P2 presentation could not resolve canonical enemy-health UI state."};

    SpriteBuffer sprites{};
    std::size_t count = 0U;
    std::uint64_t order = 0U;

    AddSprite(sprites, count, 0.0F, 0.0F, CombatGame::WorldHalfWidth, CombatGame::WorldHalfHeight,
        state->arena, -10, order);
    AddSprite(sprites, count, -7.75F, 0.0F, 0.25F, 4.5F, state->wall, -2, order);
    AddSprite(sprites, count, 7.75F, 0.0F, 0.25F, 4.5F, state->wall, -2, order);
    AddSprite(sprites, count, 0.0F, 4.25F, 8.0F, 0.25F, state->wall, -2, order);
    AddSprite(sprites, count, 0.0F, -4.25F, 8.0F, 0.25F, state->wall, -2, order);
    AddSprite(sprites, count, -0.4F, -2.25F, 1.0F, 0.40F, state->wall, -1, order);

    const trace2d::scene::Vector2 facing = state->game->Facing();
    if (state->game->HitFlashFramesRemaining() > 0U)
    {
        AddSprite(
            sprites,
            count,
            presentedPlayerPosition.x + facing.x * CombatGame::AttackReach,
            presentedPlayerPosition.y + facing.y * CombatGame::AttackReach,
            CombatGame::AttackHalfWidth,
            CombatGame::AttackHalfHeight,
            state->attack,
            1,
            order);
    }

    AddSprite(sprites, count, presentedPlayerPosition.x, presentedPlayerPosition.y,
        CombatGame::PlayerRadius, CombatGame::PlayerRadius, state->player, 3, order);
    AddSprite(sprites, count, presentedEnemyPosition.x, presentedEnemyPosition.y,
        CombatGame::EnemyRadius, CombatGame::EnemyRadius,
        state->game->HitFlashFramesRemaining() > 0U ? state->enemyFlash : state->enemy,
        3, order);

    const std::uint32_t currentHealth = enemyHealth->progress.Value();
    for (std::uint32_t index = 0U; index < CombatGame::MaximumEnemyHealth; ++index)
    {
        const bool full = index < currentHealth;
        AddSprite(sprites, count, 5.55F + static_cast<float>(index) * 0.75F, -3.75F,
            0.28F, 0.16F, full ? state->healthFull : state->healthEmpty, 10, order);
    }

    trace2d::render::OrthographicCamera camera{};
    camera.center = {0.0F, 0.0F};
    camera.verticalSize = CombatGame::WorldHalfHeight * 2.0F;
    const std::span<const trace2d::render::SpriteRenderData> view{sprites.data(), count};
    if (state->captureRequested)
    {
        const std::filesystem::path output{"trace2d-p2-combat-proof.bmp"};
        static_cast<void>(state->renderer->CaptureFrame(
            trace2d::render::CaptureRequest{
                state->captureFrame++,
                output,
                trace2d::render::CaptureImageFormat::Bmp},
            camera,
            view));
        std::cout << "Captured " << output.string() << '\n';
        state->captureRequested = false;
    }
    else
    {
        state->renderer->RenderFrame(camera, view);
    }
}

void RequireOutput(const trace2d::audio::AudioOutputResult2D result, const char* const message)
{
    if (result != trace2d::audio::AudioOutputResult2D::Success) throw std::runtime_error{message};
}
} // namespace

int main()
{
    try
    {
        trace2d::examples::EnsureP2GeneratedHitSfx(TRACE2D_P2_RUNTIME_DIR);

        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(CombatGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(CombatGame::CanvasHeight);
        platformConfig.windowTitle =
            "Trace2D P2 Combat Proof | WASD move | Space attack | R restart | C capture | Esc quit";
        trace2d::platform::Platform platform{platformConfig};

        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.025F, .green = 0.03F, .blue = 0.045F, .alpha = 1.0F};
        rendererConfig.enableDebugValidation = true;
        trace2d::render::Renderer renderer{rendererConfig, platform};

        RegistryTypes types{};
        CombatGame game{types.physics, types.audio, TRACE2D_P2_RUNTIME_DIR};
        trace2d::application::Application application{game, types.registry, MakeConfig()};
        application.Start();

        std::vector<TextureHandle> textures{};
        textures.reserve(8U);
        const auto retain = [&textures](const TextureHandle texture) {
            textures.push_back(texture);
            return texture;
        };

        PresentationState presentation{
            .renderer = &renderer,
            .game = &game,
            .arena = retain(CreateSolidTexture(game.Resources(), renderer, "p2/arena.rgba8", {28U, 38U, 50U, 255U})),
            .wall = retain(CreateSolidTexture(game.Resources(), renderer, "p2/wall.rgba8", {77U, 88U, 105U, 255U})),
            .player = retain(CreateSolidTexture(game.Resources(), renderer, "p2/player.rgba8", {82U, 190U, 218U, 255U})),
            .enemy = retain(CreateSolidTexture(game.Resources(), renderer, "p2/enemy.rgba8", {218U, 76U, 92U, 255U})),
            .enemyFlash = retain(CreateSolidTexture(game.Resources(), renderer, "p2/enemy-flash.rgba8", {255U, 231U, 154U, 255U})),
            .attack = retain(CreateSolidTexture(game.Resources(), renderer, "p2/attack.rgba8", {243U, 181U, 72U, 125U})),
            .healthFull = retain(CreateSolidTexture(game.Resources(), renderer, "p2/health-full.rgba8", {236U, 82U, 97U, 255U})),
            .healthEmpty = retain(CreateSolidTexture(game.Resources(), renderer, "p2/health-empty.rgba8", {69U, 59U, 70U, 255U})),
        };
        application.SetPresentationCallback(&Present, &presentation);

        trace2d::audio::AudioOutputConfig2D outputConfig{};
        outputConfig.voiceCapacity = 4U;
        outputConfig.preloadCacheCapacity = 2U;
        outputConfig.preloadPcmByteBudget = 2U * 1024U * 1024U;
        outputConfig.refillBufferByteBudget = 512U * 1024U;
        trace2d::audio::AudioOutput2D output{game.Resources(), outputConfig};
        RequireOutput(output.Start(), "P2 could not start the physical audio output.");

        // SDL GPU claims a window with VSYNC presentation by default. Drive authoritative gameplay
        // from measured wall time and let FixedStepRuntime consume complete 60 Hz steps. Presentation
        // interpolates previous/current fixed body positions using the remaining runtime accumulator,
        // so monitor refresh rates that do not match 60 Hz do not expose the fixed-step staircase.
        // Clamp long host stalls so a focus change or debugger pause cannot trigger a large catch-up.
        constexpr auto MaximumHostElapsed = std::chrono::milliseconds{250};
        auto previousHostTime = std::chrono::steady_clock::now();

        bool quit = false;
        while (!quit)
        {
            bool captureRequested = false;
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                {
                    quit = true;
                    continue;
                }
                if (event.type != trace2d::platform::PlatformEventType::Input) continue;
                if (event.input.control == trace2d::input::InputControl::Escape &&
                    event.input.type == trace2d::input::InputEventType::Press)
                {
                    quit = true;
                    continue;
                }
                if (event.input.control == trace2d::input::InputControl::KeyC &&
                    event.input.type == trace2d::input::InputEventType::Press)
                {
                    captureRequested = true;
                    continue;
                }
                application.ApplyInput(event.input);
            }
            if (quit) break;

            const auto currentHostTime = std::chrono::steady_clock::now();
            const auto measuredElapsed =
                std::chrono::duration_cast<std::chrono::nanoseconds>(currentHostTime - previousHostTime);
            previousHostTime = currentHostTime;
            const auto boundedElapsed = std::min(
                measuredElapsed,
                std::chrono::duration_cast<std::chrono::nanoseconds>(MaximumHostElapsed));
            static_cast<void>(application.AdvanceElapsed(boundedElapsed));

            const trace2d::audio::AudioOutputSyncReport2D sync = output.Sync(game.Audio(), game.Audio().Events());
            RequireOutput(sync.result, "P2 physical audio semantic sync failed.");
            game.Audio().ClearEvents();

            const trace2d::audio::AudioOutputDeviceEventReport2D deviceEvents = output.PollDeviceEvents();
            RequireOutput(deviceEvents.result, "P2 physical audio device polling failed.");
            if (deviceEvents.recoveryRequested)
                RequireOutput(output.Recover(game.Audio()), "P2 physical audio device recovery failed.");
            const trace2d::audio::AudioOutputPumpReport2D pump = output.Pump();
            RequireOutput(pump.result, "P2 physical audio preparation/refill pump failed.");

            presentation.captureRequested = presentation.captureRequested || captureRequested;
            static_cast<void>(application.Present());
        }

        const trace2d::physics::PhysicsMetrics2D physicsMetrics = game.PhysicsMetrics();
        const trace2d::audio::AudioMetrics2D semanticAudioMetrics = game.AudioMetrics();
        const trace2d::audio::AudioOutputMetrics2D outputMetrics = output.Metrics();
        const trace2d::render::RenderMetrics renderMetrics = renderer.Metrics();
        std::cout
            << "P2 owner metrics: hits=" << game.AcceptedHitCount()
            << ", physics_steps=" << physicsMetrics.fixedStepCount
            << ", overlap_queries=" << physicsMetrics.overlapQueryCount
            << ", semantic_audio_commands=" << semanticAudioMetrics.commandCount
            << ", physical_streams_created=" << outputMetrics.streamCreateCount
            << ", preload_bytes=" << outputMetrics.trace2dOwnedPreloadPcmBytes
            << ", draw_calls=" << renderMetrics.drawCalls
            << '\n';

        output.Stop();
        application.Stop();
        for (auto it = textures.rbegin(); it != textures.rend(); ++it)
        {
            renderer.DestroyTexture(*it);
            static_cast<void>(game.Resources().Unload(it->Untyped()));
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Trace2D P2 windowed owner proof failed: " << error.what() << '\n';
        return 1;
    }
}