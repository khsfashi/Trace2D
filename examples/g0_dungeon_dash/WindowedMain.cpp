#include "DungeonDashGame.hpp"

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/assets/TextureAssets.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef TRACE2D_DUNGEON_DASH_ROOT
#define TRACE2D_DUNGEON_DASH_ROOT "."
#endif

namespace
{
using Clock = std::chrono::steady_clock;
using Color = std::array<std::uint8_t, 4U>;
using TextureHandle = trace2d::render::TextureHandle;
using SpriteBuffer = std::array<trace2d::render::SpriteRenderData, 256U>;

struct PresentationState final
{
    trace2d::render::Renderer* renderer{nullptr};
    DungeonDashGame* game{nullptr};
    TextureHandle floor{};
    TextureHandle floorAlt{};
    TextureHandle player{};
    TextureHandle hunter{};
    TextureHandle wall{};
    TextureHandle relic{};
    TextureHandle relicCollected{};
    TextureHandle running{};
    TextureHandle won{};
    TextureHandle lost{};
};

TextureHandle LoadTexture(
    trace2d::assets::TextureAssetCache& cache,
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const std::string& reference)
{
    const trace2d::assets::TextureAssetLoadResult loaded = cache.Load(reference);
    if (!loaded.Succeeded())
    {
        std::string message = "Dungeon Dash could not load texture '" + reference + "'.";
        if (loaded.diagnostic.has_value() && !loaded.diagnostic->message.empty())
        {
            message.append(" ");
            message.append(loaded.diagnostic->message);
        }
        throw std::runtime_error{message};
    }

    trace2d::assets::TextureResource canonical{};
    canonical.width = loaded.asset->width;
    canonical.height = loaded.asset->height;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Srgb;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Dungeon Dash keeps source pixels for the lifetime of the playable example.";
    canonical.canonicalRgba8 = loaded.asset->rgba8;

    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Dungeon Dash could not publish an imported texture."};

    trace2d::render::Rgba8TextureData data{};
    data.width = loaded.asset->width;
    data.height = loaded.asset->height;
    data.pixels = std::span<const std::uint8_t>{loaded.asset->rgba8.data(), loaded.asset->rgba8.size()};
    return renderer.CreateSpriteTextureRgba8(
        published.handle,
        data,
        trace2d::render::SpriteTextureEncoding::Srgb);
}

TextureHandle CreateSolidTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    std::string reference,
    const Color color)
{
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonical.retentionReason = "Dungeon Dash generated presentation color.";
    canonical.canonicalRgba8.assign(color.begin(), color.end());

    const auto published = resources.PublishTexture(std::move(reference), std::move(canonical));
    if (!published.Succeeded())
        throw std::runtime_error{"Dungeon Dash could not publish a generated color texture."};

    trace2d::render::Rgba8TextureData data{};
    data.width = 1U;
    data.height = 1U;
    data.pixels = std::span<const std::uint8_t>{color.data(), color.size()};
    return renderer.CreateTextureRgba8(published.handle, data);
}

void ReleaseTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const TextureHandle texture) noexcept
{
    renderer.DestroyTexture(texture);
    static_cast<void>(resources.Unload(texture.Untyped()));
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
    if (count >= sprites.size()) throw std::runtime_error{"Dungeon Dash presentation sprite capacity exceeded."};
    sprites[count++] = trace2d::render::SpriteRenderData{
        .center = {.x = x, .y = y},
        .halfExtents = {.x = halfWidth, .y = halfHeight},
        .texture = texture,
        .layer = layer,
        .stableOrder = order++,
    };
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr || state->game == nullptr)
        throw std::runtime_error{"Dungeon Dash presentation state is unavailable."};

    const trace2d::scene::Entity* const player = context.Scene().TryGet(state->game->Player());
    const trace2d::scene::Entity* const hunter = context.Scene().TryGet(state->game->Hunter());
    if (player == nullptr || hunter == nullptr)
        throw std::runtime_error{"Dungeon Dash presentation could not resolve player/hunter state."};

    SpriteBuffer sprites{};
    std::size_t count = 0U;
    std::uint64_t order = 0U;

    // 16 x 9 world-space tiles map exactly to the 16:9 camera. At 768 x 432 each
    // source 16 px Kenney tile is displayed at an integer 3x scale.
    for (int row = 0; row < 9; ++row)
    {
        for (int column = 0; column < 16; ++column)
        {
            const float x = -7.5F + static_cast<float>(column);
            const float y = -4.0F + static_cast<float>(row);
            const bool accent = ((row * 7 + column * 3) % 17) == 0;
            AddSprite(sprites, count, x, y, 0.5F, 0.5F, accent ? state->floorAlt : state->floor, -20, order);
        }
    }

    AddSprite(sprites, count, -7.82F, 0.0F, 0.18F, 4.5F, state->wall, -5, order);
    AddSprite(sprites, count, 7.82F, 0.0F, 0.18F, 4.5F, state->wall, -5, order);
    AddSprite(sprites, count, 0.0F, -4.32F, 8.0F, 0.18F, state->wall, -5, order);
    AddSprite(sprites, count, 0.0F, 4.32F, 8.0F, 0.18F, state->wall, -5, order);

    for (std::size_t index = 0U; index < DungeonDashGame::RelicCount; ++index)
    {
        const trace2d::scene::Entity* const relicEntity = context.Scene().TryGet(state->game->Relic(index));
        if (relicEntity == nullptr) throw std::runtime_error{"Dungeon Dash presentation lost a relic entity."};
        if (state->game->RelicCollected(index)) continue;

        const auto position = relicEntity->LocalTransform().position;
        AddSprite(sprites, count, position.x, position.y, 0.34F, 0.34F, state->relic, 1, order);
        AddSprite(sprites, count, position.x, position.y, 0.19F, 0.19F, state->floorAlt, 2, order);
    }

    AddSprite(
        sprites,
        count,
        hunter->LocalTransform().position.x,
        hunter->LocalTransform().position.y,
        0.50F,
        0.50F,
        state->hunter,
        4,
        order);
    AddSprite(
        sprites,
        count,
        player->LocalTransform().position.x,
        player->LocalTransform().position.y,
        0.50F,
        0.50F,
        state->player,
        5,
        order);

    // Five small deterministic markers are enough HUD for this tiny game without introducing
    // another text/font dependency. They mirror the canonical collected flags exactly.
    for (std::size_t index = 0U; index < DungeonDashGame::RelicCount; ++index)
    {
        AddSprite(
            sprites,
            count,
            -7.15F + static_cast<float>(index) * 0.55F,
            3.82F,
            0.20F,
            0.20F,
            state->game->RelicCollected(index) ? state->relicCollected : state->relic,
            20,
            order);
    }

    TextureHandle statusTexture = state->running;
    if (state->game->CurrentState() == DungeonDashGame::State::Won) statusTexture = state->won;
    else if (state->game->CurrentState() == DungeonDashGame::State::Lost) statusTexture = state->lost;
    AddSprite(sprites, count, 7.15F, 3.82F, 0.28F, 0.18F, statusTexture, 20, order);

    if (state->game->CurrentState() != DungeonDashGame::State::Running)
    {
        AddSprite(sprites, count, 0.0F, 4.10F, 7.4F, 0.07F, statusTexture, 30, order);
        AddSprite(sprites, count, 0.0F, -4.10F, 7.4F, 0.07F, statusTexture, 30, order);
        AddSprite(sprites, count, -7.55F, 0.0F, 0.07F, 3.9F, statusTexture, 30, order);
        AddSprite(sprites, count, 7.55F, 0.0F, 0.07F, 3.9F, statusTexture, 30, order);
    }

    trace2d::render::OrthographicCamera camera{};
    camera.center = {.x = 0.0F, .y = 0.0F};
    camera.verticalSize = DungeonDashGame::WorldHalfHeight * 2.0F;
    state->renderer->RenderFrame(
        camera,
        std::span<const trace2d::render::SpriteRenderData>{sprites.data(), count});
}
} // namespace

int main()
{
    using namespace std::chrono_literals;
    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(DungeonDashGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(DungeonDashGame::CanvasHeight);
        platformConfig.windowTitle = "Trace2D G0 - Dungeon Dash | WASD move | collect 5 relics | avoid hunter | R restart | Esc quit";
        trace2d::platform::Platform platform{platformConfig};

        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.02F, .green = 0.025F, .blue = 0.035F, .alpha = 1.0F};
        trace2d::render::Renderer renderer{rendererConfig, platform};
        trace2d::assets::ResourceRegistry resources{TRACE2D_DUNGEON_DASH_ROOT};
        trace2d::assets::TextureAssetCache textureCache{TRACE2D_DUNGEON_DASH_ROOT};

        std::vector<TextureHandle> textures{};
        textures.reserve(10U);
        const auto retain = [&textures](const TextureHandle texture) {
            textures.push_back(texture);
            return texture;
        };

        DungeonDashGame game{};
        PresentationState presentation{
            .renderer = &renderer,
            .game = &game,
            .floor = retain(LoadTexture(
                textureCache, resources, renderer, "assets/kenney-tiny-dungeon/floor.png")),
            .floorAlt = retain(LoadTexture(
                textureCache, resources, renderer, "assets/kenney-tiny-dungeon/floor-alt.png")),
            .player = retain(LoadTexture(
                textureCache, resources, renderer, "assets/kenney-tiny-dungeon/player.png")),
            .hunter = retain(LoadTexture(
                textureCache, resources, renderer, "assets/kenney-tiny-dungeon/hunter.png")),
            .wall = retain(CreateSolidTexture(resources, renderer, "dungeon-dash/wall.rgba8", {38U, 46U, 61U, 255U})),
            .relic = retain(CreateSolidTexture(resources, renderer, "dungeon-dash/relic.rgba8", {236U, 184U, 65U, 255U})),
            .relicCollected = retain(CreateSolidTexture(resources, renderer, "dungeon-dash/relic-collected.rgba8", {88U, 151U, 102U, 255U})),
            .running = retain(CreateSolidTexture(resources, renderer, "dungeon-dash/running.rgba8", {74U, 139U, 184U, 255U})),
            .won = retain(CreateSolidTexture(resources, renderer, "dungeon-dash/won.rgba8", {81U, 190U, 116U, 255U})),
            .lost = retain(CreateSolidTexture(resources, renderer, "dungeon-dash/lost.rgba8", {211U, 72U, 83U, 255U})),
        };

        trace2d::application::ApplicationConfig applicationConfig{};
        applicationConfig.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
        applicationConfig.runtime.seed = 427U;
        applicationConfig.scene.semanticId = "g0.dungeon-dash";
        applicationConfig.scene.name = "Dungeon Dash";
        applicationConfig.uiWidth = static_cast<std::uint32_t>(DungeonDashGame::CanvasWidth);
        applicationConfig.uiHeight = static_cast<std::uint32_t>(DungeonDashGame::CanvasHeight);

        trace2d::application::Application application{game, applicationConfig};
        application.SetPresentationCallback(&Present, &presentation);
        application.Start();

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
            static_cast<void>(application.Present());
            std::this_thread::sleep_for(1ms);
        }
        application.Stop();

        for (auto it = textures.rbegin(); it != textures.rend(); ++it)
            ReleaseTexture(resources, renderer, *it);
        return 0;
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
