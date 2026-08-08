#include "PublicAlphaCommand.hpp"

#include <trace2d/input/Input.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/testing/GameplayScenario.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace trace2d::tools
{
namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitRuntimeFailure = 3;
constexpr int ExitSceneLoadFailure = 4;
constexpr int ExitAssertionFailure = 5;

constexpr std::uint64_t DefaultFrameCount = 8U;
constexpr std::uint64_t DefaultSeed = 42U;
constexpr std::uint64_t MovePressFrame = 2U;
constexpr std::uint64_t MoveReleaseFrame = 6U;
constexpr std::uint32_t MeasurementWidth = 1280U;
constexpr std::uint32_t MeasurementHeight = 720U;
constexpr std::size_t MaxSampleSprites = 16U;

constexpr std::array<std::uint8_t, 16> PlayerPixels{
    255, 96, 96, 255,
    255, 180, 96, 255,
    255, 180, 96, 255,
    255, 96, 96, 255,
};

constexpr std::array<std::uint8_t, 16> MarkerPixels{
    72, 170, 255, 255,
    72, 120, 220, 255,
    72, 120, 220, 255,
    72, 170, 255, 255,
};

std::string EscapeJson(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());

    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }

    return escaped;
}

bool TryParseUnsigned64(const std::string_view text, std::uint64_t& value) noexcept
{
    if (text.empty())
    {
        return false;
    }

    value = 0U;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ReadTextFile(const std::string& path, std::string& text)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        return false;
    }

    std::ostringstream buffer{};
    buffer << input.rdbuf();
    if (input.bad())
    {
        return false;
    }

    text = buffer.str();
    return true;
}

void MovePlayerWhileRightHeld(testing::GameplayFrameContext& context)
{
    if (!context.input.Held(input::InputControl::KeyD))
    {
        return;
    }

    const auto playerId = context.scene.FindBySemanticId("player");
    if (!playerId.has_value())
    {
        throw std::logic_error{"Public Alpha sample is missing #player."};
    }

    scene::Entity* const player = context.scene.TryGet(*playerId);
    if (player == nullptr)
    {
        throw std::logic_error{"Public Alpha sample returned a stale #player handle."};
    }

    player->Transform().position.x += 1.0F;
}

bool TryGetPlayerX(const scene::Scene& scene, float& playerX) noexcept
{
    const auto playerId = scene.FindBySemanticId("player");
    if (!playerId.has_value())
    {
        return false;
    }

    const scene::Entity* const player = scene.TryGet(*playerId);
    if (player == nullptr)
    {
        return false;
    }

    playerX = player->Transform().position.x;
    return true;
}

std::uint64_t MovementFrameCount(const std::uint64_t frameCount) noexcept
{
    if (frameCount < MovePressFrame)
    {
        return 0U;
    }

    const std::uint64_t finalHeldFrame = std::min(frameCount, MoveReleaseFrame - 1U);
    return finalHeldFrame - MovePressFrame + 1U;
}

bool BuildSampleSprites(
    const scene::Scene& scene,
    const render::TextureHandle playerTexture,
    const render::TextureHandle markerTexture,
    std::array<render::SpriteRenderData, MaxSampleSprites>& sprites,
    std::size_t& spriteCount) noexcept
{
    spriteCount = 0U;
    bool overflow = false;

    scene.ForEachEntity(
        [&](const scene::EntityId, const scene::Entity& entity)
        {
            if (overflow)
            {
                return;
            }

            if (spriteCount >= sprites.size())
            {
                overflow = true;
                return;
            }

            const bool isPlayer = entity.SemanticId() == "player";
            render::SpriteRenderData& sprite = sprites[spriteCount];
            sprite.center = render::Float2{
                entity.Transform().position.x,
                entity.Transform().position.y,
            };
            sprite.halfExtents = isPlayer ? render::Float2{0.6F, 0.6F} : render::Float2{0.4F, 0.4F};
            sprite.texture = isPlayer ? playerTexture : markerTexture;
            sprite.layer = isPlayer ? 1 : 0;
            sprite.stableOrder = static_cast<std::uint64_t>(spriteCount);
            ++spriteCount;
        });

    return !overflow;
}

void PrintSceneDiagnostics(const testing::GameplaySceneLoadResult& loadResult)
{
    for (const scene::SceneTextDiagnostic& diagnostic : loadResult.diagnostics)
    {
        std::cerr << "  " << diagnostic.path << ": " << diagnostic.message;
        if (diagnostic.line != 0U)
        {
            std::cerr << " (" << diagnostic.line << ':' << diagnostic.column << ')';
        }
        std::cerr << '\n';
    }
}
} // namespace

int RunPublicAlphaCommand(const int argc, char* argv[])
{
    bool json = false;
    bool modeSelected = false;
    bool captureRequested = false;
    std::uint64_t frameCount = DefaultFrameCount;
    std::uint64_t seed = DefaultSeed;
    std::string scenePath{};
    std::string capturePath{};
    platform::StartupMode mode = platform::StartupMode::Headless;

    for (int index = 2; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if (argument == "--json")
        {
            json = true;
            continue;
        }

        if (argument == "--headless" || argument == "--windowed")
        {
            const platform::StartupMode requestedMode =
                argument == "--headless" ? platform::StartupMode::Headless : platform::StartupMode::Windowed;

            if (modeSelected && mode != requestedMode)
            {
                std::cerr << "Select exactly one startup mode: --headless or --windowed.\n";
                return ExitUsage;
            }

            mode = requestedMode;
            modeSelected = true;
            continue;
        }

        if (argument == "--scene")
        {
            if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty())
            {
                std::cerr << "--scene requires a scene path.\n";
                return ExitUsage;
            }

            scenePath = argv[++index];
            continue;
        }

        if (argument == "--frames" || argument == "--seed")
        {
            if (index + 1 >= argc)
            {
                std::cerr << argument << " requires an unsigned integer value.\n";
                return ExitUsage;
            }

            std::uint64_t parsedValue = 0U;
            if (!TryParseUnsigned64(argv[index + 1], parsedValue))
            {
                std::cerr << "Invalid value for " << argument << ": " << argv[index + 1] << '\n';
                return ExitUsage;
            }

            if (argument == "--frames")
            {
                frameCount = parsedValue;
            }
            else
            {
                seed = parsedValue;
            }

            ++index;
            continue;
        }

        if (argument == "--capture")
        {
            if (captureRequested || index + 1 >= argc || std::string_view{argv[index + 1]}.empty())
            {
                std::cerr << "--capture requires exactly one artifact path.\n";
                return ExitUsage;
            }

            capturePath = argv[++index];
            captureRequested = true;
            continue;
        }

        std::cerr << "Unknown public-alpha option: " << argument << '\n';
        return ExitUsage;
    }

    if (!modeSelected)
    {
        std::cerr << "public-alpha requires --headless or --windowed.\n";
        return ExitUsage;
    }

    if (scenePath.empty())
    {
        std::cerr << "public-alpha requires --scene PATH.\n";
        return ExitUsage;
    }

    if (captureRequested && mode != platform::StartupMode::Windowed)
    {
        std::cerr << "--capture requires --windowed.\n";
        return ExitUsage;
    }

    std::string sceneText{};
    if (!ReadTextFile(scenePath, sceneText))
    {
        std::cerr << "Unable to read Public Alpha scene: " << scenePath << '\n';
        return ExitSceneLoadFailure;
    }

    runtime::RuntimeConfig runtimeConfig{};
    runtimeConfig.seed = seed;
    testing::GameplayScenario scenario{runtimeConfig};
    const testing::GameplaySceneLoadResult loadResult = scenario.LoadSceneToml(sceneText, scenePath);
    if (!loadResult.Succeeded())
    {
        std::cerr << "Public Alpha scene failed validation.\n";
        PrintSceneDiagnostics(loadResult);
        return ExitSceneLoadFailure;
    }

    const scene::Scene* const baselineScene = scenario.ActiveScene();
    float initialPlayerX = 0.0F;
    if (baselineScene == nullptr || !TryGetPlayerX(*baselineScene, initialPlayerX))
    {
        std::cerr << "Public Alpha scene requires exactly one authored entity with semantic ID #player.\n";
        return ExitSceneLoadFailure;
    }

    try
    {
        scenario.SchedulePress(MovePressFrame, input::InputControl::KeyD);
        scenario.ScheduleRelease(MoveReleaseFrame, input::InputControl::KeyD);
        scenario.RunFrames(frameCount, MovePlayerWhileRightHeld);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Public Alpha deterministic run failed: " << exception.what() << '\n';
        return ExitRuntimeFailure;
    }

    const float expectedPlayerX = initialPlayerX + static_cast<float>(MovementFrameCount(frameCount));
    const bool assertionPassed = scenario.AssertFloatFieldEquals(
        "#player",
        "Transform2D",
        "position.x",
        expectedPlayerX);

    const scene::Scene* const activeScene = scenario.ActiveScene();
    float playerX = 0.0F;
    if (activeScene == nullptr || !TryGetPlayerX(*activeScene, playerX))
    {
        std::cerr << "Public Alpha run lost #player state.\n";
        return ExitRuntimeFailure;
    }

    std::array<render::SpriteRenderData, MaxSampleSprites> measurementSprites{};
    std::size_t spriteCount = 0U;
    if (!BuildSampleSprites(*activeScene, 1U, 2U, measurementSprites, spriteCount))
    {
        std::cerr << "Public Alpha sample exceeds its fixed sprite capacity of " << MaxSampleSprites << ".\n";
        return ExitRuntimeFailure;
    }

    render::OrthographicCamera camera{};
    camera.verticalSize = 8.0F;

    render::OrthographicView measurementView{};
    if (!render::TryBuildOrthographicView(camera, MeasurementWidth, MeasurementHeight, measurementView))
    {
        std::cerr << "Unable to build the Public Alpha batching measurement view.\n";
        return ExitRuntimeFailure;
    }

    const render::SpriteBatchMeasurement batching = render::MeasureContiguousTextureBatching(
        measurementView,
        std::span<const render::SpriteRenderData>{measurementSprites.data(), spriteCount});
    const std::uint64_t estimatedDrawCallSaving =
        batching.visibleSprites >= batching.contiguousTextureRuns
            ? batching.visibleSprites - batching.contiguousTextureRuns
            : 0U;

    render::RenderMetrics renderMetrics{};
    render::CapturedFrame capturedFrame{};

    if (mode == platform::StartupMode::Windowed)
    {
        try
        {
            platform::PlatformConfig platformConfig{};
            platformConfig.mode = platform::StartupMode::Windowed;
            platform::Platform platformInstance{platformConfig};
            render::Renderer renderer{render::RendererConfig{}, platformInstance};

            render::Rgba8TextureData playerTextureData{};
            playerTextureData.width = 2U;
            playerTextureData.height = 2U;
            playerTextureData.pixels = std::span<const std::uint8_t>{PlayerPixels};

            render::Rgba8TextureData markerTextureData{};
            markerTextureData.width = 2U;
            markerTextureData.height = 2U;
            markerTextureData.pixels = std::span<const std::uint8_t>{MarkerPixels};

            const render::TextureHandle playerTexture = renderer.CreateTextureRgba8(playerTextureData);
            const render::TextureHandle markerTexture = renderer.CreateTextureRgba8(markerTextureData);

            std::array<render::SpriteRenderData, MaxSampleSprites> renderSprites{};
            std::size_t renderSpriteCount = 0U;
            if (!BuildSampleSprites(*activeScene, playerTexture, markerTexture, renderSprites, renderSpriteCount))
            {
                std::cerr << "Public Alpha sample exceeds its fixed render sprite capacity.\n";
                return ExitRuntimeFailure;
            }

            const std::span<const render::SpriteRenderData> renderSpan{renderSprites.data(), renderSpriteCount};
            if (captureRequested)
            {
                render::CaptureRequest request{};
                request.simulationFrame = scenario.Runtime().State().frame;
                request.artifactPath = capturePath;
                capturedFrame = renderer.CaptureFrame(request, camera, renderSpan);
            }
            else
            {
                renderer.RenderFrame(camera, renderSpan);
            }

            renderMetrics = renderer.Metrics();
            renderer.DestroyTexture(markerTexture);
            renderer.DestroyTexture(playerTexture);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Public Alpha windowed render failed: " << exception.what() << '\n';
            return ExitRuntimeFailure;
        }
    }

    if (!assertionPassed)
    {
        std::cerr << "Public Alpha exact-frame assertion failed for #player Transform2D.position.x.\n";
        return ExitAssertionFailure;
    }

    if (json)
    {
        std::cout << "{\"command\":\"public-alpha\",\"status\":\"ok\",\"mode\":\""
                  << platform::ToString(mode)
                  << "\",\"scene\":\"" << EscapeJson(scenePath)
                  << "\",\"frame\":" << scenario.Runtime().State().frame
                  << ",\"seed\":" << scenario.Runtime().State().seed
                  << ",\"selector\":\"#player\""
                  << ",\"input\":{\"press_frame\":" << MovePressFrame
                  << ",\"release_frame\":" << MoveReleaseFrame << "}"
                  << ",\"assertion_passed\":true"
                  << ",\"player_x\":" << playerX
                  << ",\"expected_player_x\":" << expectedPlayerX
                  << ",\"visible_sprites\":" << batching.visibleSprites
                  << ",\"culled_sprites\":" << batching.culledSprites
                  << ",\"contiguous_texture_runs\":" << batching.contiguousTextureRuns
                  << ",\"estimated_draw_call_saving\":" << estimatedDrawCallSaving;

        if (mode == platform::StartupMode::Windowed)
        {
            std::cout << ",\"rendered_frames\":" << renderMetrics.presentedFrames
                      << ",\"draw_calls\":" << renderMetrics.drawCalls
                      << ",\"submitted_sprites\":" << renderMetrics.submittedSprites;
        }

        if (captureRequested)
        {
            std::cout << ",\"capture_frame\":" << capturedFrame.simulationFrame
                      << ",\"capture_width\":" << capturedFrame.width
                      << ",\"capture_height\":" << capturedFrame.height
                      << ",\"capture_path\":\"" << EscapeJson(capturePath) << "\"";
        }

        std::cout << "}\n";
        return ExitSuccess;
    }

    std::cout << "Trace2D Public Alpha vertical sample\n"
              << "  mode: " << platform::ToString(mode) << '\n'
              << "  scene: " << scenePath << '\n'
              << "  frame: " << scenario.Runtime().State().frame << '\n'
              << "  seed: " << scenario.Runtime().State().seed << '\n'
              << "  controlled entity: #player\n"
              << "  virtual input: KeyD press @ " << MovePressFrame << ", release @ " << MoveReleaseFrame << '\n'
              << "  player x: " << playerX << " (expected " << expectedPlayerX << ")\n"
              << "  exact-frame assertion: passed\n"
              << "  visible sprites: " << batching.visibleSprites << '\n'
              << "  contiguous texture runs: " << batching.contiguousTextureRuns << '\n'
              << "  measured draw-call saving candidate: " << estimatedDrawCallSaving << '\n';

    if (mode == platform::StartupMode::Windowed)
    {
        std::cout << "  rendered frames: " << renderMetrics.presentedFrames << '\n'
                  << "  actual draw calls: " << renderMetrics.drawCalls << '\n';
    }

    if (captureRequested)
    {
        std::cout << "  capture frame: " << capturedFrame.simulationFrame << '\n'
                  << "  capture path: " << capturePath << '\n';
    }

    std::cout << "  status: ok\n";
    return ExitSuccess;
}
} // namespace trace2d::tools
