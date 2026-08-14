#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/RendererWorkload.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#ifndef TRACE2D_BUILD_CONFIGURATION
#define TRACE2D_BUILD_CONFIGURATION "unknown"
#endif

#define TRACE2D_STRINGIFY_IMPL(value) #value
#define TRACE2D_STRINGIFY(value) TRACE2D_STRINGIFY_IMPL(value)

namespace
{
struct Options final
{
    bool list{false};
    bool timing{false};
    std::string workloadName{};
    std::uint32_t iterations{120};
    std::uint32_t warmupFrames{30};
    std::string machineLabel{};
    std::string gpuModel{};
    std::string driverVersion{};
};

struct MetricsDelta final
{
    std::uint64_t submittedFrames{0};
    std::uint64_t presentedFrames{0};
    std::uint64_t renderPasses{0};
    std::uint64_t drawCalls{0};
    std::uint64_t submittedSprites{0};
    std::uint64_t culledSprites{0};
};

[[nodiscard]] constexpr trace2d::render::TextureHandle MeasurementTextureHandle(
    const std::uint32_t slot) noexcept
{
    return trace2d::render::TextureHandle{
        slot,
        1U,
        trace2d::assets::ResourceTypeDomain::Texture,
    };
}

void WriteJsonString(std::ostream& output, const std::string_view value)
{
    output << '"';
    for (const char character : value)
    {
        switch (character)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            output << character;
            break;
        }
    }
    output << '"';
}

[[nodiscard]] bool TryParsePositiveU32(const std::string_view text, std::uint32_t& outValue) noexcept
{
    std::uint64_t parsed = 0;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed == 0 ||
        parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    outValue = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool TryParseNonNegativeU32(const std::string_view text, std::uint32_t& outValue) noexcept
{
    std::uint64_t parsed = 0;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end ||
        parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    outValue = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] Options ParseOptions(const int argc, char** const argv)
{
    Options options{};

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if (argument == "--list")
        {
            options.list = true;
            continue;
        }
        if (argument == "--timing")
        {
            options.timing = true;
            continue;
        }

        const auto requireValue = [&](const char* const optionName) -> std::string_view {
            if (index + 1 >= argc)
            {
                throw std::invalid_argument{std::string{optionName} + " requires a value."};
            }
            ++index;
            return std::string_view{argv[index]};
        };

        if (argument == "--workload")
        {
            options.workloadName = requireValue("--workload");
        }
        else if (argument == "--iterations")
        {
            const std::string_view value = requireValue("--iterations");
            if (!TryParsePositiveU32(value, options.iterations))
            {
                throw std::invalid_argument{"--iterations must be a positive 32-bit integer."};
            }
        }
        else if (argument == "--warmup")
        {
            const std::string_view value = requireValue("--warmup");
            if (!TryParseNonNegativeU32(value, options.warmupFrames))
            {
                throw std::invalid_argument{"--warmup must be a non-negative 32-bit integer."};
            }
        }
        else if (argument == "--machine-label")
        {
            options.machineLabel = requireValue("--machine-label");
        }
        else if (argument == "--gpu-model")
        {
            options.gpuModel = requireValue("--gpu-model");
        }
        else if (argument == "--driver-version")
        {
            options.driverVersion = requireValue("--driver-version");
        }
        else
        {
            throw std::invalid_argument{"Unknown renderer workload option: " + std::string{argument}};
        }
    }

    if (!options.list && options.workloadName.empty())
    {
        options.list = true;
    }

    if (options.list && options.timing)
    {
        throw std::invalid_argument{"--list and --timing cannot be combined."};
    }

    if (options.timing &&
        (options.machineLabel.empty() || options.gpuModel.empty() || options.driverVersion.empty()))
    {
        throw std::invalid_argument{
            "--timing requires --machine-label, --gpu-model, and --driver-version metadata."};
    }

    return options;
}

[[nodiscard]] std::string_view OperatingSystemName() noexcept
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string_view CompilerId() noexcept
{
#if defined(_MSC_VER)
    return "msvc";
#elif defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string_view CompilerVersion() noexcept
{
#if defined(_MSC_FULL_VER)
    return TRACE2D_STRINGIFY(_MSC_FULL_VER);
#elif defined(__clang_version__)
    return __clang_version__;
#elif defined(__VERSION__)
    return __VERSION__;
#else
    return "unknown";
#endif
}

void WriteStructure(
    std::ostream& output,
    const trace2d::render::RendererWorkload& workload,
    const trace2d::render::RendererWorkloadStructure& structure)
{
    output << "{\"name\":";
    WriteJsonString(output, workload.spec.name);
    output << ",\"target_width\":" << workload.spec.targetWidth;
    output << ",\"target_height\":" << workload.spec.targetHeight;
    output << ",\"camera_center_x\":" << workload.spec.camera.center.x;
    output << ",\"camera_center_y\":" << workload.spec.camera.center.y;
    output << ",\"camera_vertical_size\":" << workload.spec.camera.verticalSize;
    output << ",\"authored_sprites\":" << structure.authoredSprites;
    output << ",\"visible_sprites\":" << structure.visibleSprites;
    output << ",\"culled_sprites\":" << structure.culledSprites;
    output << ",\"contiguous_texture_runs\":" << structure.contiguousTextureRuns;
    output << '}';
}

[[nodiscard]] MetricsDelta BuildMetricsDelta(
    const trace2d::render::RenderMetrics& before,
    const trace2d::render::RenderMetrics& after)
{
    if (after.submittedFrames < before.submittedFrames ||
        after.presentedFrames < before.presentedFrames ||
        after.renderPasses < before.renderPasses ||
        after.drawCalls < before.drawCalls ||
        after.submittedSprites < before.submittedSprites ||
        after.culledSprites < before.culledSprites)
    {
        throw std::logic_error{"Renderer metrics are not monotonic."};
    }

    return MetricsDelta{
        after.submittedFrames - before.submittedFrames,
        after.presentedFrames - before.presentedFrames,
        after.renderPasses - before.renderPasses,
        after.drawCalls - before.drawCalls,
        after.submittedSprites - before.submittedSprites,
        after.culledSprites - before.culledSprites,
    };
}

int RunList()
{
    constexpr std::array<trace2d::render::TextureHandle, 2> PlaceholderTextures{
        MeasurementTextureHandle(0U),
        MeasurementTextureHandle(1U),
    };

    std::cout << "{\"command\":\"renderer-workload-list\",\"metric_source\":\"deterministic_structure\",\"workloads\":[";
    bool first = true;
    for (const trace2d::render::RendererWorkloadSpec& spec : trace2d::render::RendererWorkloadSpecs())
    {
        const trace2d::render::RendererWorkload workload =
            trace2d::render::BuildRendererWorkload(spec.id, PlaceholderTextures);
        const trace2d::render::RendererWorkloadStructure structure =
            trace2d::render::MeasureRendererWorkloadStructure(workload);

        if (!first)
        {
            std::cout << ',';
        }
        first = false;
        WriteStructure(std::cout, workload, structure);
    }
    std::cout << "],\"status\":\"ok\"}\n";
    return 0;
}

int RunStructure(const trace2d::render::RendererWorkloadId id)
{
    constexpr std::array<trace2d::render::TextureHandle, 2> PlaceholderTextures{
        MeasurementTextureHandle(0U),
        MeasurementTextureHandle(1U),
    };
    const trace2d::render::RendererWorkload workload =
        trace2d::render::BuildRendererWorkload(id, PlaceholderTextures);
    const trace2d::render::RendererWorkloadStructure structure =
        trace2d::render::MeasureRendererWorkloadStructure(workload);

    std::cout << "{\"command\":\"renderer-workload\",\"metric_source\":\"deterministic_structure\",\"workload\":";
    WriteStructure(std::cout, workload, structure);
    std::cout << ",\"status\":\"ok\"}\n";
    return 0;
}

int RunTiming(const Options& options, const trace2d::render::RendererWorkloadId id)
{
    const trace2d::render::RendererWorkloadSpec* spec = nullptr;
    for (const trace2d::render::RendererWorkloadSpec& candidate : trace2d::render::RendererWorkloadSpecs())
    {
        if (candidate.id == id)
        {
            spec = &candidate;
            break;
        }
    }
    if (spec == nullptr)
    {
        throw std::logic_error{"Renderer workload spec was not found."};
    }

    trace2d::platform::PlatformConfig platformConfig{};
    platformConfig.mode = trace2d::platform::StartupMode::Windowed;
    platformConfig.windowWidth = static_cast<int>(spec->targetWidth);
    platformConfig.windowHeight = static_cast<int>(spec->targetHeight);
    platformConfig.windowTitle = "Trace2D Renderer Workload";

    trace2d::platform::Platform platform{platformConfig};
    trace2d::render::Renderer renderer{trace2d::render::RendererConfig{}, platform};
    trace2d::assets::ResourceRegistry resources{"."};

    constexpr std::array<std::uint8_t, 4> WhitePixel{255, 255, 255, 255};
    constexpr std::array<std::uint8_t, 4> MagentaPixel{255, 0, 255, 255};
    const trace2d::render::Rgba8TextureData firstTextureData{1U, 1U, WhitePixel};
    const trace2d::render::Rgba8TextureData secondTextureData{1U, 1U, MagentaPixel};

    trace2d::assets::TextureResource firstCanonical{};
    firstCanonical.width = 1U;
    firstCanonical.height = 1U;
    firstCanonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    firstCanonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    firstCanonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Reacquirable;
    firstCanonical.retentionReason = "renderer workload pixels are built-in";
    firstCanonical.canonicalRgba8.assign(WhitePixel.begin(), WhitePixel.end());
    const auto firstPublished = resources.PublishTexture(
        "runtime/renderer-workload-white.rgba8",
        std::move(firstCanonical));
    if (!firstPublished.Succeeded())
    {
        throw std::runtime_error{"Failed to publish renderer workload white texture."};
    }

    trace2d::assets::TextureResource secondCanonical{};
    secondCanonical.width = 1U;
    secondCanonical.height = 1U;
    secondCanonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    secondCanonical.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    secondCanonical.cpuRetention = trace2d::assets::CpuRetentionPolicy::Reacquirable;
    secondCanonical.retentionReason = "renderer workload pixels are built-in";
    secondCanonical.canonicalRgba8.assign(MagentaPixel.begin(), MagentaPixel.end());
    const auto secondPublished = resources.PublishTexture(
        "runtime/renderer-workload-magenta.rgba8",
        std::move(secondCanonical));
    if (!secondPublished.Succeeded())
    {
        throw std::runtime_error{"Failed to publish renderer workload magenta texture."};
    }

    const trace2d::render::TextureHandle firstTexture = renderer.CreateTextureRgba8(
        firstPublished.handle,
        firstTextureData);
    const trace2d::render::TextureHandle secondTexture = renderer.CreateTextureRgba8(
        secondPublished.handle,
        secondTextureData);
    const std::array<trace2d::render::TextureHandle, 2> textureSlots{firstTexture, secondTexture};

    const trace2d::render::RendererWorkload workload =
        trace2d::render::BuildRendererWorkload(id, textureSlots);
    const trace2d::render::RendererWorkloadStructure structure =
        trace2d::render::MeasureRendererWorkloadStructure(workload);

    for (std::uint32_t frame = 0; frame < options.warmupFrames; ++frame)
    {
        renderer.RenderFrame(workload.spec.camera, workload.sprites);
    }

    const trace2d::render::RenderMetrics before = renderer.Metrics();
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t frame = 0; frame < options.iterations; ++frame)
    {
        renderer.RenderFrame(workload.spec.camera, workload.sprites);
    }
    const auto end = std::chrono::steady_clock::now();
    const trace2d::render::RenderMetrics after = renderer.Metrics();

    const MetricsDelta delta = BuildMetricsDelta(before, after);
    const std::int64_t totalWallNanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const double averageWallNanoseconds =
        static_cast<double>(totalWallNanoseconds) / static_cast<double>(options.iterations);

    const std::uint64_t expectedDrawCalls =
        structure.contiguousTextureRuns * static_cast<std::uint64_t>(options.iterations);
    const std::uint64_t expectedSubmittedSprites =
        structure.visibleSprites * static_cast<std::uint64_t>(options.iterations);
    const std::uint64_t expectedCulledSprites =
        structure.culledSprites * static_cast<std::uint64_t>(options.iterations);

    const bool submissionMatches =
        delta.submittedFrames == options.iterations &&
        delta.renderPasses == options.iterations &&
        delta.drawCalls == expectedDrawCalls &&
        delta.submittedSprites == expectedSubmittedSprites &&
        delta.culledSprites == expectedCulledSprites;

    std::cout << "{\"command\":\"renderer-workload\",\"metric_source\":\"successful_gpu_submission\",\"workload\":";
    WriteStructure(std::cout, workload, structure);
    std::cout << ",\"iterations\":" << options.iterations;
    std::cout << ",\"warmup_frames\":" << options.warmupFrames;
    std::cout << ",\"submission_delta\":{\"submitted_frames\":" << delta.submittedFrames;
    std::cout << ",\"presented_frames\":" << delta.presentedFrames;
    std::cout << ",\"render_passes\":" << delta.renderPasses;
    std::cout << ",\"draw_calls\":" << delta.drawCalls;
    std::cout << ",\"submitted_sprites\":" << delta.submittedSprites;
    std::cout << ",\"culled_sprites\":" << delta.culledSprites << '}';
    std::cout << ",\"timing\":{\"scope\":\"cpu_wall_clock_renderframe_submission\",\"total_ns\":"
              << totalWallNanoseconds << ",\"average_ns\":" << averageWallNanoseconds << '}';
    std::cout << ",\"environment\":{\"machine_label\":";
    WriteJsonString(std::cout, options.machineLabel);
    std::cout << ",\"gpu_model\":";
    WriteJsonString(std::cout, options.gpuModel);
    std::cout << ",\"gpu_driver_version\":";
    WriteJsonString(std::cout, options.driverVersion);
    std::cout << ",\"renderer_backend\":";
    WriteJsonString(std::cout, renderer.DriverName());
    std::cout << ",\"os\":";
    WriteJsonString(std::cout, OperatingSystemName());
    std::cout << ",\"compiler_id\":";
    WriteJsonString(std::cout, CompilerId());
    std::cout << ",\"compiler_version\":";
    WriteJsonString(std::cout, CompilerVersion());
    std::cout << ",\"build_configuration\":";
    WriteJsonString(std::cout, TRACE2D_BUILD_CONFIGURATION);
    std::cout << "},\"status\":\"" << (submissionMatches ? "ok" : "submission_mismatch") << "\"}\n";

    renderer.DestroyTexture(secondTexture);
    renderer.DestroyTexture(firstTexture);
    if (!resources.Unload(secondTexture.Untyped()).Succeeded() ||
        !resources.Unload(firstTexture.Untyped()).Succeeded())
    {
        throw std::runtime_error{"Failed to unload renderer workload texture resources."};
    }
    return submissionMatches ? 0 : 3;
}

int Run(const int argc, char** const argv)
{
    const Options options = ParseOptions(argc, argv);
    if (options.list)
    {
        return RunList();
    }

    trace2d::render::RendererWorkloadId id = trace2d::render::RendererWorkloadId::DenseSingleTexture;
    if (!trace2d::render::TryParseRendererWorkloadId(options.workloadName, id))
    {
        throw std::invalid_argument{"Unknown renderer workload name: " + options.workloadName};
    }

    return options.timing ? RunTiming(options, id) : RunStructure(id);
}
} // namespace

int main(const int argc, char** const argv)
{
    try
    {
        return Run(argc, argv);
    }
    catch (const std::exception& exception)
    {
        std::cout << "{\"command\":\"renderer-workload\",\"status\":\"error\",\"message\":";
        WriteJsonString(std::cout, exception.what());
        std::cout << "}\n";
        return 2;
    }
}