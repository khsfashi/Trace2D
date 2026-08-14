#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/ui/UiRaster.hpp>
#include <trace2d/ui/UiText.hpp>

#include <charconv>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitRuntimeFailure = 3;

void PrintHelp()
{
    std::cout << "Trace2D authored UI preview\n\n"
              << "Usage:\n"
              << "  trace2d_ui_preview (--headless|--windowed) --ui PATH [--frames N] [--json]\n";
}

[[nodiscard]] std::string EscapeJson(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());

    for (const unsigned char byte : value)
    {
        switch (byte)
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
            if (byte >= 0x20U)
            {
                escaped += static_cast<char>(byte);
            }
            break;
        }
    }

    return escaped;
}

[[nodiscard]] bool TryParsePositiveUnsigned64(
    const std::string_view text,
    std::uint64_t& value) noexcept
{
    if (text.empty())
    {
        return false;
    }

    value = 0U;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end && value > 0U;
}

[[nodiscard]] bool ReadTextFile(const std::string& path, std::string& text)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        return false;
    }

    text.assign(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

int PrintFailure(
    const bool json,
    const std::string_view stage,
    const std::string_view message)
{
    if (json)
    {
        std::cerr << "{\"command\":\"ui-preview\",\"status\":\"error\",\"stage\":\""
                  << EscapeJson(stage) << "\",\"message\":\"" << EscapeJson(message) << "\"}\n";
    }
    else
    {
        std::cerr << "UI preview failed during " << stage << ": " << message << '\n';
    }
    return ExitRuntimeFailure;
}
} // namespace

int main(const int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintHelp();
        return ExitUsage;
    }

    bool json = false;
    bool modeSelected = false;
    bool windowed = false;
    std::string uiPath{};
    std::uint64_t frameCount = 1U;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--json")
        {
            json = true;
            continue;
        }

        if (argument == "--headless" || argument == "--windowed")
        {
            const bool requestedWindowed = argument == "--windowed";
            if (modeSelected && windowed != requestedWindowed)
            {
                std::cerr << "Select exactly one startup mode: --headless or --windowed.\n";
                return ExitUsage;
            }

            windowed = requestedWindowed;
            modeSelected = true;
            continue;
        }

        if (argument == "--ui")
        {
            if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty())
            {
                std::cerr << "--ui requires a TOML file path.\n";
                return ExitUsage;
            }

            uiPath = argv[index + 1];
            ++index;
            continue;
        }

        if (argument == "--frames")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "--frames requires a positive unsigned integer.\n";
                return ExitUsage;
            }

            const std::string_view valueText{argv[index + 1]};
            if (!TryParsePositiveUnsigned64(valueText, frameCount))
            {
                std::cerr << "Invalid --frames value: " << valueText << '\n';
                return ExitUsage;
            }

            ++index;
            continue;
        }

        if (argument == "--help" || argument == "-h")
        {
            PrintHelp();
            return ExitSuccess;
        }

        std::cerr << "Unknown option: " << argument << '\n';
        return ExitUsage;
    }

    if (!modeSelected)
    {
        std::cerr << "An explicit startup mode is required: --headless or --windowed.\n";
        return ExitUsage;
    }

    if (uiPath.empty())
    {
        std::cerr << "--ui PATH is required.\n";
        return ExitUsage;
    }

    std::string authoredText{};
    if (!ReadTextFile(uiPath, authoredText))
    {
        return PrintFailure(json, "read", "Could not read the authored UI file.");
    }

    trace2d::ui::UiLoadResult loadResult = trace2d::ui::LoadUiToml(authoredText, uiPath);
    if (!loadResult.Succeeded() || !loadResult.document.has_value())
    {
        if (!loadResult.diagnostics.empty())
        {
            const trace2d::ui::UiTextDiagnostic& diagnostic = loadResult.diagnostics.front();
            std::string message = diagnostic.path + ": " + diagnostic.message;
            if (diagnostic.line > 0U)
            {
                message += " (line " + std::to_string(diagnostic.line) + ", column " +
                           std::to_string(diagnostic.column) + ')';
            }
            return PrintFailure(json, "parse", message);
        }

        return PrintFailure(json, "parse", "Authored UI could not be loaded.");
    }

    trace2d::ui::UiDocument document = std::move(*loadResult.document);
    trace2d::ui::UiRasterImage raster{};
    trace2d::ui::UiRasterMetrics rasterMetrics{};
    if (!trace2d::ui::RasterizeUi(document, raster, &rasterMetrics))
    {
        return PrintFailure(json, "raster", "UI rasterization failed.");
    }

    if (!windowed)
    {
        if (json)
        {
            std::cout << "{\"command\":\"ui-preview\",\"mode\":\"headless\",\"ui\":\""
                      << EscapeJson(uiPath) << "\",\"width\":" << raster.width
                      << ",\"height\":" << raster.height
                      << ",\"elements\":" << rasterMetrics.elementsRasterized
                      << ",\"glyphs\":" << rasterMetrics.glyphsRasterized
                      << ",\"rgba_bytes\":" << raster.rgba8.size()
                      << ",\"status\":\"ok\"}\n";
        }
        else
        {
            std::cout << "Trace2D UI preview\n"
                      << "  mode: headless\n"
                      << "  ui: " << uiPath << '\n'
                      << "  canvas: " << raster.width << 'x' << raster.height << '\n'
                      << "  elements: " << rasterMetrics.elementsRasterized << '\n'
                      << "  glyphs: " << rasterMetrics.glyphsRasterized << '\n'
                      << "  rgba bytes: " << raster.rgba8.size() << '\n'
                      << "  status: ok\n";
        }
        return ExitSuccess;
    }

    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        trace2d::platform::Platform platform{platformConfig};
        trace2d::render::Renderer renderer{trace2d::render::RendererConfig{}, platform};
        trace2d::assets::ResourceRegistry resources{"."};

        trace2d::render::Rgba8TextureData textureData{};
        textureData.width = raster.width;
        textureData.height = raster.height;
        textureData.pixels = std::span<const std::uint8_t>{raster.rgba8.data(), raster.rgba8.size()};

        trace2d::assets::TextureResource canonicalTexture{};
        canonicalTexture.width = raster.width;
        canonicalTexture.height = raster.height;
        canonicalTexture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
        canonicalTexture.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
        canonicalTexture.cpuRetention = trace2d::assets::CpuRetentionPolicy::Reacquirable;
        canonicalTexture.retentionReason = "UI preview raster can be regenerated from authored UI";
        canonicalTexture.canonicalRgba8 = raster.rgba8;
        const auto publishedTexture = resources.PublishTexture(
            "runtime/ui-preview.rgba8",
            std::move(canonicalTexture));
        if (!publishedTexture.Succeeded())
        {
            return PrintFailure(json, "resource", "Could not publish UI preview texture resource.");
        }

        const trace2d::render::TextureHandle texture =
            renderer.CreateTextureRgba8(publishedTexture.handle, textureData);

        const float halfWidth = static_cast<float>(document.Width()) * 0.5F;
        const float halfHeight = static_cast<float>(document.Height()) * 0.5F;
        trace2d::render::OrthographicCamera camera{};
        camera.center = trace2d::render::Float2{halfWidth, halfHeight};
        camera.verticalSize = static_cast<float>(document.Height());

        trace2d::render::SpriteRenderData sprite{};
        sprite.center = camera.center;
        sprite.halfExtents = trace2d::render::Float2{halfWidth, halfHeight};
        sprite.texture = texture;

        bool quitRequested = false;
        for (std::uint64_t frame = 0U; frame < frameCount && !quitRequested; ++frame)
        {
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                {
                    quitRequested = true;
                    break;
                }
            }

            if (!quitRequested)
            {
                renderer.RenderFrame(camera, sprite);
            }
        }

        renderer.DestroyTexture(texture);
        static_cast<void>(resources.Unload(texture.Untyped()));

        if (json)
        {
            std::cout << "{\"command\":\"ui-preview\",\"mode\":\"windowed\",\"ui\":\""
                      << EscapeJson(uiPath) << "\",\"width\":" << raster.width
                      << ",\"height\":" << raster.height
                      << ",\"elements\":" << rasterMetrics.elementsRasterized
                      << ",\"glyphs\":" << rasterMetrics.glyphsRasterized
                      << ",\"rendered_frames\":" << renderer.Metrics().presentedFrames
                      << ",\"draw_calls\":" << renderer.Metrics().drawCalls
                      << ",\"status\":\"ok\"}\n";
        }
        else
        {
            std::cout << "Trace2D UI preview\n"
                      << "  mode: windowed\n"
                      << "  ui: " << uiPath << '\n'
                      << "  canvas: " << raster.width << 'x' << raster.height << '\n'
                      << "  elements: " << rasterMetrics.elementsRasterized << '\n'
                      << "  glyphs: " << rasterMetrics.glyphsRasterized << '\n'
                      << "  rendered frames: " << renderer.Metrics().presentedFrames << '\n'
                      << "  draw calls: " << renderer.Metrics().drawCalls << '\n'
                      << "  status: ok\n";
        }

        return ExitSuccess;
    }
    catch (const std::exception& exception)
    {
        return PrintFailure(json, "windowed", exception.what());
    }
}
