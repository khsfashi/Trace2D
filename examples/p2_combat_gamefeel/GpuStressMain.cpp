#include "CombatGame.hpp"
#include "P2GeneratedAssets.hpp"

#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/physics/PhysicsComponents2D.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/ui/Ui.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef TRACE2D_P2_RUNTIME_DIR
#define TRACE2D_P2_RUNTIME_DIR "."
#endif

#ifndef TRACE2D_XSTRESS_SOURCE_REVISION
#define TRACE2D_XSTRESS_SOURCE_REVISION "unknown"
#endif

namespace
{
using trace2d::examples::CombatGame;
using TextureHandle = trace2d::render::TextureHandle;
using Color = std::array<std::uint8_t, 4U>;
using SpriteBuffer = std::array<trace2d::render::SpriteRenderData, 32U>;

constexpr std::uint64_t SampleFrameCount = 90U;
constexpr std::uint64_t ScenarioSeed = 329U;
constexpr std::string_view WorkloadName = "trace2d.p2-owner-gpu-stress.v1";

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
    std::filesystem::path capturePath{};
    bool captureRequested{false};
    trace2d::render::CapturedFrame captured{};
};

[[nodiscard]] trace2d::application::ApplicationConfig MakeConfig()
{
    trace2d::application::ApplicationConfig config{};
    config.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
    config.runtime.seed = ScenarioSeed;
    config.scene.semanticId = "p2.combat-gamefeel";
    config.scene.name = "P2 Combat Game Feel";
    config.uiWidth = static_cast<std::uint32_t>(CombatGame::CanvasWidth);
    config.uiHeight = static_cast<std::uint32_t>(CombatGame::CanvasHeight);
    return config;
}

void ScheduleScenario(trace2d::application::Application& application)
{
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
    application.ScheduleInput(85U, {
        .control = trace2d::input::InputControl::Space,
        .type = trace2d::input::InputEventType::Press,
    });
    application.ScheduleInput(86U, {
        .control = trace2d::input::InputControl::Space,
        .type = trace2d::input::InputEventType::Release,
    });
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
    if (!published.Succeeded())
    {
        throw std::runtime_error{"XSTRESS2 could not publish a presentation texture."};
    }

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
    if (count >= sprites.size())
    {
        throw std::runtime_error{"XSTRESS2 presentation sprite capacity exceeded."};
    }
    sprites[count++] = trace2d::render::SpriteRenderData{
        .center = {x, y},
        .halfExtents = {halfWidth, halfHeight},
        .texture = texture,
        .layer = layer,
        .stableOrder = order++,
    };
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr || state->game == nullptr)
    {
        throw std::runtime_error{"XSTRESS2 presentation state is unavailable."};
    }

    trace2d::physics::PhysicsBodyState2D player{};
    trace2d::physics::PhysicsBodyState2D enemy{};
    if (!state->game->TryPlayerBodyState(player) || !state->game->TryEnemyBodyState(enemy))
    {
        throw std::runtime_error{"XSTRESS2 could not inspect authoritative body state."};
    }

    const trace2d::ui::UiElement* const enemyHealth = context.Ui().Find("hud.enemy-health");
    if (enemyHealth == nullptr || !enemyHealth->progress.Active())
    {
        throw std::runtime_error{"XSTRESS2 could not resolve canonical enemy-health UI state."};
    }

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
            player.position.x + facing.x * CombatGame::AttackReach,
            player.position.y + facing.y * CombatGame::AttackReach,
            CombatGame::AttackHalfWidth,
            CombatGame::AttackHalfHeight,
            state->attack,
            1,
            order);
    }

    AddSprite(sprites, count, player.position.x, player.position.y,
        CombatGame::PlayerRadius, CombatGame::PlayerRadius, state->player, 3, order);
    AddSprite(sprites, count, enemy.position.x, enemy.position.y,
        CombatGame::EnemyRadius, CombatGame::EnemyRadius,
        state->game->HitFlashFramesRemaining() > 0U ? state->enemyFlash : state->enemy,
        3, order);

    const std::uint32_t currentHealth = enemyHealth->progress.Value();
    for (std::uint32_t index = 0U; index < CombatGame::MaximumEnemyHealth; ++index)
    {
        AddSprite(sprites, count, 5.55F + static_cast<float>(index) * 0.75F, -3.75F,
            0.28F, 0.16F, index < currentHealth ? state->healthFull : state->healthEmpty, 10, order);
    }

    trace2d::render::OrthographicCamera camera{};
    camera.center = {0.0F, 0.0F};
    camera.verticalSize = CombatGame::WorldHalfHeight * 2.0F;
    const std::span<const trace2d::render::SpriteRenderData> view{sprites.data(), count};

    if (state->captureRequested)
    {
        state->captured = state->renderer->CaptureFrame(
            trace2d::render::CaptureRequest{
                SampleFrameCount,
                state->capturePath,
                trace2d::render::CaptureImageFormat::Bmp},
            camera,
            view);
        state->captureRequested = false;
    }
    else
    {
        state->renderer->RenderFrame(camera, view);
    }
}

void ValidateScenario(const CombatGame& game, const trace2d::render::RenderMetrics& renderer)
{
    const auto physics = game.PhysicsMetrics();
    const auto audio = game.AudioMetrics();
    const auto resources = game.ResourceStats();

    if (game.EnemyHealth() != CombatGame::MaximumEnemyHealth - 1U || game.AcceptedHitCount() != 1U ||
        game.AcceptedSwingCount() != 1U || game.SemanticStartedEventCount() != 1U)
    {
        throw std::runtime_error{"XSTRESS2 authoritative combat result changed."};
    }
    if (physics.attachedBodyCount != 7U || physics.fixedStepCount != SampleFrameCount ||
        physics.overlapQueryCount != 1U || physics.overlapCapacityFailureCount != 0U ||
        physics.bodyCommandFailureCount != 0U)
    {
        throw std::runtime_error{"XSTRESS2 Physics2D structural result changed."};
    }
    if (audio.commandFailureCount != 0U || resources.readyResources < 1U || resources.filesystemQueries != 0U)
    {
        throw std::runtime_error{"XSTRESS2 audio/resource structural result changed."};
    }
    if (renderer.submittedFrames != SampleFrameCount || renderer.renderPasses != SampleFrameCount ||
        renderer.drawCalls == 0U || renderer.submittedSprites == 0U)
    {
        throw std::runtime_error{"XSTRESS2 renderer did not retain the expected real-GPU frame workload."};
    }
    if (renderer.explicitGpuReadbacks != 1U || renderer.explicitGpuFenceWaits != 1U)
    {
        throw std::runtime_error{"XSTRESS2 explicit capture boundary changed."};
    }
}

void WriteJsonString(std::ostream& output, const std::string_view value)
{
    output << '"';
    for (const char character : value)
    {
        switch (character)
        {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << character; break;
        }
    }
    output << '"';
}

void WriteEvidence(
    const std::filesystem::path& evidencePath,
    const std::filesystem::path& capturePath,
    const std::string_view backend,
    const CombatGame& game,
    const trace2d::render::RenderMetrics& renderer,
    const trace2d::render::CapturedFrame& captured)
{
    std::ofstream output{evidencePath, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error{"XSTRESS2 could not open evidence output."};
    }

    const auto physics = game.PhysicsMetrics();
    const auto audio = game.AudioMetrics();
    const auto resources = game.ResourceStats();

    output << "{\"schema\":\"trace2d.xstress2.p2-gpu.v1\",\"status\":\"passed\",\"source_revision\":";
    WriteJsonString(output, TRACE2D_XSTRESS_SOURCE_REVISION);
    output << ",\"workload\":";
    WriteJsonString(output, WorkloadName);
    output << ",\"seed\":" << ScenarioSeed
           << ",\"fixed_timestep_ns\":16666667"
           << ",\"frames\":" << SampleFrameCount
           << ",\"renderer_backend\":";
    WriteJsonString(output, backend);
    output << ",\"gpu_timing\":{\"availability\":\"not_supported\"}"
           << ",\"combat\":{\"enemy_health\":" << game.EnemyHealth()
           << ",\"accepted_hits\":" << game.AcceptedHitCount()
           << ",\"accepted_swings\":" << game.AcceptedSwingCount()
           << ",\"semantic_started_events\":" << game.SemanticStartedEventCount() << '}'
           << ",\"physics\":{\"attached_bodies\":" << physics.attachedBodyCount
           << ",\"fixed_steps\":" << physics.fixedStepCount
           << ",\"overlap_queries\":" << physics.overlapQueryCount
           << ",\"body_command_failures\":" << physics.bodyCommandFailureCount
           << ",\"overlap_capacity_failures\":" << physics.overlapCapacityFailureCount << '}'
           << ",\"audio\":{\"commands\":" << audio.commandCount
           << ",\"command_failures\":" << audio.commandFailureCount
           << ",\"retained_voice_capacity\":" << audio.retainedVoiceCapacity
           << ",\"retained_event_capacity\":" << audio.retainedEventCapacity << '}'
           << ",\"resources\":{\"ready\":" << resources.readyResources
           << ",\"filesystem_queries\":" << resources.filesystemQueries << '}'
           << ",\"renderer\":{\"submitted_frames\":" << renderer.submittedFrames
           << ",\"presented_frames\":" << renderer.presentedFrames
           << ",\"render_passes\":" << renderer.renderPasses
           << ",\"draw_calls\":" << renderer.drawCalls
           << ",\"submitted_sprites\":" << renderer.submittedSprites
           << ",\"culled_sprites\":" << renderer.culledSprites
           << ",\"sprite_vertex_capacity_sprites\":" << renderer.spriteVertexCapacitySprites
           << ",\"sprite_vertex_capacity_bytes\":" << renderer.spriteVertexCapacityBytes
           << ",\"explicit_gpu_readbacks\":" << renderer.explicitGpuReadbacks
           << ",\"explicit_gpu_fence_waits\":" << renderer.explicitGpuFenceWaits
           << ",\"retained_offscreen_color_target_bytes\":" << renderer.retainedOffscreenColorTargetBytes
           << ",\"target_width\":" << renderer.lastTargetWidth
           << ",\"target_height\":" << renderer.lastTargetHeight << '}'
           << ",\"capture\":{\"path\":";
    WriteJsonString(output, capturePath.generic_string());
    output << ",\"simulation_frame\":" << captured.simulationFrame
           << ",\"width\":" << captured.width
           << ",\"height\":" << captured.height
           << ",\"rgba8_bytes\":" << captured.rgba8Pixels.size() << "}}\n";

    if (!output)
    {
        throw std::runtime_error{"XSTRESS2 could not write evidence output."};
    }
}

[[nodiscard]] bool Enabled() noexcept
{
    const char* const value = std::getenv("TRACE2D_RUN_GPU_SMOKE");
    return value != nullptr && std::string_view{value} == "1";
}
} // namespace

int main(const int argc, char* argv[])
{
    try
    {
        if (!Enabled())
        {
            std::cout << "XSTRESS2 owner real-GPU proof disabled outside explicit GPU validation.\n";
            return 0;
        }
        if (argc != 2)
        {
            std::cerr << "usage: trace2d_p2_combat_gamefeel_gpu_stress <evidence-directory>\n";
            return 2;
        }

        const std::filesystem::path outputDirectory{argv[1]};
        std::filesystem::create_directories(outputDirectory);
        const std::filesystem::path capturePath = outputDirectory / "p2-owner-gpu-final.bmp";
        const std::filesystem::path evidencePath = outputDirectory / "p2-owner-gpu-evidence.json";

        trace2d::examples::EnsureP2GeneratedHitSfx(TRACE2D_P2_RUNTIME_DIR);

        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(CombatGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(CombatGame::CanvasHeight);
        platformConfig.windowTitle = "Trace2D XSTRESS2 P2 owner real-GPU proof";
        trace2d::platform::Platform platform{platformConfig};

        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.025F, .green = 0.03F, .blue = 0.045F, .alpha = 1.0F};
        rendererConfig.enableDebugValidation = false;
        trace2d::render::Renderer renderer{rendererConfig, platform};

        RegistryTypes types{};
        CombatGame game{types.physics, types.audio, TRACE2D_P2_RUNTIME_DIR};
        trace2d::application::Application application{game, types.registry, MakeConfig()};
        ScheduleScenario(application);
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
            .arena = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/arena.rgba8", {28U, 38U, 50U, 255U})),
            .wall = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/wall.rgba8", {77U, 88U, 105U, 255U})),
            .player = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/player.rgba8", {82U, 190U, 218U, 255U})),
            .enemy = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/enemy.rgba8", {218U, 76U, 92U, 255U})),
            .enemyFlash = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/enemy-flash.rgba8", {255U, 231U, 154U, 255U})),
            .attack = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/attack.rgba8", {243U, 181U, 72U, 125U})),
            .healthFull = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/health-full.rgba8", {236U, 82U, 97U, 255U})),
            .healthEmpty = retain(CreateSolidTexture(game.Resources(), renderer, "p2/xstress2/health-empty.rgba8", {69U, 59U, 70U, 255U})),
            .capturePath = capturePath,
        };
        application.SetPresentationCallback(&Present, &presentation);

        for (std::uint64_t frame = 1U; frame <= SampleFrameCount; ++frame)
        {
            application.StepFrames(1U);
            presentation.captureRequested = frame == SampleFrameCount;
            static_cast<void>(application.Present());
        }

        const trace2d::render::RenderMetrics rendererMetrics = renderer.Metrics();
        ValidateScenario(game, rendererMetrics);
        if (presentation.captured.simulationFrame != SampleFrameCount ||
            presentation.captured.width == 0U || presentation.captured.height == 0U ||
            presentation.captured.rgba8Pixels.empty() || !std::filesystem::is_regular_file(capturePath))
        {
            throw std::runtime_error{"XSTRESS2 final-frame capture evidence is incomplete."};
        }

        WriteEvidence(
            evidencePath,
            capturePath,
            renderer.DriverName(),
            game,
            rendererMetrics,
            presentation.captured);
        application.Stop();

        std::cout << "TRACE2D_XSTRESS2_V1 status=passed workload=" << WorkloadName
                  << " frames=" << SampleFrameCount
                  << " backend=" << renderer.DriverName()
                  << " submitted_frames=" << rendererMetrics.submittedFrames
                  << " draw_calls=" << rendererMetrics.drawCalls
                  << " submitted_sprites=" << rendererMetrics.submittedSprites
                  << " readbacks=" << rendererMetrics.explicitGpuReadbacks
                  << " fence_waits=" << rendererMetrics.explicitGpuFenceWaits
                  << " evidence=" << evidencePath.string() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Trace2D XSTRESS2 P2 GPU proof failed: " << error.what() << '\n';
        return 30;
    }
}
