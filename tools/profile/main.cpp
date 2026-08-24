#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/core/Version.hpp>
#include <trace2d/particles/ParticleReference.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/profile/Profile2D.hpp>
#include <trace2d/profile/ProfileReport2D.hpp>
#include <trace2d/profile/StructuralProfile2D.hpp>
#include <trace2d/profile_adapters/ProfileAdapters2D.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifndef TRACE2D_PROFILE_SOURCE_REVISION
#define TRACE2D_PROFILE_SOURCE_REVISION "unknown"
#endif

namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitRuntimeFailure = 3;

constexpr std::uint64_t DefaultSampleFrameCount = 120U;
constexpr std::uint64_t DefaultWarmupFrameCount = 8U;
constexpr std::uint64_t DefaultSeed = 42U;
constexpr std::uint64_t MaximumTotalFrameCount = 10'000U;
constexpr std::size_t MaximumRetainedCpuFrameCount = 240U;
constexpr std::uint32_t ParticleCapacity = 64U;
constexpr std::uint64_t ProfileEmitterStableId = 0x5452414345324450ULL;
constexpr std::string_view WorkloadName = "trace2d.representative-profile.v1";
constexpr std::string_view TimingSource = "std::chrono::steady_clock";

constexpr std::array<std::uint8_t, 16U> SampleTexturePixels{
    255U, 96U, 64U, 255U,
    64U, 192U, 255U, 255U,
    255U, 220U, 64U, 255U,
    160U, 96U, 255U, 255U,
};

struct Options final
{
    trace2d::platform::StartupMode mode{trace2d::platform::StartupMode::Headless};
    std::uint64_t sampleFrameCount{DefaultSampleFrameCount};
    std::uint64_t warmupFrameCount{DefaultWarmupFrameCount};
    std::uint64_t seed{DefaultSeed};
    std::string outputPath{};
    bool modeSelected{false};
    bool json{false};
};

[[nodiscard]] std::string_view BuildConfiguration() noexcept
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

[[nodiscard]] std::string_view OperatingSystem() noexcept
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

[[nodiscard]] std::string_view Architecture() noexcept
{
#if defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string_view Compiler() noexcept
{
#if defined(__clang__)
    return "clang";
#elif defined(_MSC_VER)
    return "msvc";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unknown";
#endif
}

void PrintHelp()
{
    std::cout
        << "Trace2D representative profiler\n\n"
        << "Usage:\n"
        << "  trace2d-profile (--headless|--windowed) [--frames N] [--warmup N] [--seed N] [--output PATH] [--json]\n\n"
        << "The workload uses the public fixed-step runtime, ResourceRegistry and deterministic\n"
        << "ParticleReference APIs. Windowed mode additionally measures the public renderer path.\n"
        << "CPU timings are environment-labelled evidence only; hosted CI gates structural metrics,\n"
        << "not wall-clock thresholds.\n";
}

[[nodiscard]] bool TryParseUnsigned64(const std::string_view text, std::uint64_t& value) noexcept
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

[[nodiscard]] bool ParseOptions(const int argc, char* argv[], Options& options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h")
        {
            PrintHelp();
            return false;
        }
        if (argument == "--json")
        {
            options.json = true;
            continue;
        }
        if (argument == "--headless" || argument == "--windowed")
        {
            const trace2d::platform::StartupMode requestedMode =
                argument == "--headless" ? trace2d::platform::StartupMode::Headless
                                         : trace2d::platform::StartupMode::Windowed;
            if (options.modeSelected && options.mode != requestedMode)
            {
                throw std::invalid_argument{"select exactly one startup mode: --headless or --windowed"};
            }
            options.mode = requestedMode;
            options.modeSelected = true;
            continue;
        }
        if (argument == "--frames" || argument == "--warmup" || argument == "--seed")
        {
            if (index + 1 >= argc)
            {
                throw std::invalid_argument{std::string{argument} + " requires an unsigned integer"};
            }
            std::uint64_t parsed = 0U;
            if (!TryParseUnsigned64(argv[index + 1], parsed))
            {
                throw std::invalid_argument{"invalid value for " + std::string{argument} + ": " + argv[index + 1]};
            }
            if (argument == "--frames")
            {
                options.sampleFrameCount = parsed;
            }
            else if (argument == "--warmup")
            {
                options.warmupFrameCount = parsed;
            }
            else
            {
                options.seed = parsed;
            }
            ++index;
            continue;
        }
        if (argument == "--output")
        {
            if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty())
            {
                throw std::invalid_argument{"--output requires a non-empty path"};
            }
            options.outputPath = argv[++index];
            continue;
        }

        throw std::invalid_argument{"unknown option: " + std::string{argument}};
    }

    if (!options.modeSelected)
    {
        throw std::invalid_argument{"an explicit startup mode is required: --headless or --windowed"};
    }
    if (options.sampleFrameCount == 0U)
    {
        throw std::invalid_argument{"--frames must be greater than zero"};
    }
    if (options.sampleFrameCount > MaximumTotalFrameCount ||
        options.warmupFrameCount > MaximumTotalFrameCount ||
        options.sampleFrameCount > MaximumTotalFrameCount - options.warmupFrameCount)
    {
        throw std::invalid_argument{"warmup + sample frames must not exceed 10000"};
    }
    return true;
}

void RequireProfileResult(const trace2d::profile::ProfileResult2D result, const std::string_view operation)
{
    if (result != trace2d::profile::ProfileResult2D::Success)
    {
        throw std::runtime_error{
            std::string{operation} + " failed: " + std::string{trace2d::profile::ToString(result)}};
    }
}

void WriteFile(const std::string& path, const std::string& contents)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error{"could not open profile output: " + path};
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.put('\n');
    if (!output)
    {
        throw std::runtime_error{"could not write profile output: " + path};
    }
}

[[nodiscard]] trace2d::particles::ParticleReferenceEmitter BuildParticleEmitter(const std::uint64_t seed)
{
    trace2d::particles::ParticleReferenceDefinition definition{};
    definition.maxParticles = ParticleCapacity;
    definition.globalSeed = seed;
    definition.emitterStableId = ProfileEmitterStableId;
    definition.periodicStartFrame = 0U;
    definition.periodicCount = 2U;
    definition.periodicEveryFrames = 4U;
    definition.lifetimeFrames = {12U, 12U};
    definition.speed = {0.25F, 0.25F};
    definition.angleRadians = {0.0F, 0.0F};
    definition.initialSize = {1.0F, 1.0F};
    definition.spriteChoiceCount = 1U;

    trace2d::particles::ParticleReferenceEmitter emitter{};
    const auto prepareResult = emitter.Prepare(
        definition,
        std::span<const trace2d::particles::ParticleBurst>{});
    if (!prepareResult.Ok())
    {
        throw std::runtime_error{"ParticleReferenceEmitter preparation failed"};
    }
    return emitter;
}

[[nodiscard]] std::string RunProfile(const Options& options)
{
    trace2d::runtime::RuntimeConfig runtimeConfig{};
    runtimeConfig.seed = options.seed;
    trace2d::runtime::FixedStepRuntime runtime{runtimeConfig};
    auto emitter = BuildParticleEmitter(options.seed);

    trace2d::assets::ResourceRegistry resources{"."};
    trace2d::assets::TextureResource canonicalTexture{};
    canonicalTexture.width = 2U;
    canonicalTexture.height = 2U;
    canonicalTexture.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonicalTexture.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    canonicalTexture.cpuRetention = trace2d::assets::CpuRetentionPolicy::Required;
    canonicalTexture.retentionReason = "representative profile workload retains its built-in source bytes";
    canonicalTexture.canonicalRgba8.assign(SampleTexturePixels.begin(), SampleTexturePixels.end());
    const auto publishedTexture = resources.PublishTexture(
        "runtime/profile-workload.rgba8",
        std::move(canonicalTexture));
    if (!publishedTexture.Succeeded())
    {
        throw std::runtime_error{"representative profile texture publication failed"};
    }

    std::unique_ptr<trace2d::platform::Platform> platform{};
    std::unique_ptr<trace2d::render::Renderer> renderer{};
    trace2d::render::TextureHandle renderTexture{};
    trace2d::render::SpriteRenderData sprite{};
    std::string rendererBackend{"headless"};

    if (options.mode == trace2d::platform::StartupMode::Windowed)
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platform = std::make_unique<trace2d::platform::Platform>(platformConfig);
        renderer = std::make_unique<trace2d::render::Renderer>(trace2d::render::RendererConfig{}, *platform);
        rendererBackend = std::string{renderer->DriverName()};

        trace2d::render::Rgba8TextureData textureData{};
        textureData.width = 2U;
        textureData.height = 2U;
        textureData.pixels = std::span<const std::uint8_t>{SampleTexturePixels};
        renderTexture = renderer->CreateTextureRgba8(publishedTexture.handle, textureData);
        sprite.texture = renderTexture;
        sprite.halfExtents = {1.0F, 1.0F};

        const auto residencyResult = resources.SetTextureRendererResidency(
            publishedTexture.handle,
            true,
            SampleTexturePixels.size());
        if (!residencyResult.Succeeded())
        {
            throw std::runtime_error{"representative profile texture residency accounting failed"};
        }
    }

    trace2d::profile::CpuProfiler2D cpuProfiler{};
    const std::size_t retainedFrameCount = static_cast<std::size_t>(
        std::min<std::uint64_t>(options.sampleFrameCount, MaximumRetainedCpuFrameCount));
    RequireProfileResult(cpuProfiler.Prepare(1U, retainedFrameCount, 1U), "CPU profiler Prepare");
    trace2d::profile::ProfileScopeId2D frameScope{};
    RequireProfileResult(cpuProfiler.RegisterScope("representative.frame", frameScope), "CPU scope registration");
    cpuProfiler.SetEnabled(true);

    trace2d::render::OrthographicCamera camera{};
    camera.verticalSize = 6.0F;

    const auto executeFrame = [&]()
    {
        runtime.Step(1U);
        if (!emitter.Step())
        {
            throw std::runtime_error{"ParticleReferenceEmitter step failed"};
        }
        if (renderer != nullptr)
        {
            renderer->RenderFrame(camera, sprite);
        }
    };

    for (std::uint64_t frame = 0U; frame < options.warmupFrameCount; ++frame)
    {
        executeFrame();
    }

    for (std::uint64_t sample = 0U; sample < options.sampleFrameCount; ++sample)
    {
        const std::uint64_t frameIndex = runtime.State().frame + 1U;
        const auto begin = trace2d::profile::SteadyProfileTimestamp2D();
        RequireProfileResult(cpuProfiler.BeginFrame(frameIndex, begin), "CPU frame begin");
        RequireProfileResult(cpuProfiler.EnterScope(frameScope, begin), "CPU scope enter");
        executeFrame();
        const auto end = trace2d::profile::SteadyProfileTimestamp2D();
        RequireProfileResult(cpuProfiler.ExitScope(frameScope, end), "CPU scope exit");
        RequireProfileResult(cpuProfiler.EndFrame(end), "CPU frame end");
    }

    const trace2d::render::RenderMetrics rendererMetrics =
        renderer != nullptr ? renderer->Metrics() : trace2d::render::RenderMetrics{};
    const trace2d::assets::ResourceRegistryStats resourceStats = resources.Stats();
    const std::vector<trace2d::assets::ResourceSnapshot> resourceSnapshots = resources.InspectAll();
    const trace2d::particles::ParticleReferenceCounters particleCounters = emitter.Counters();
    const trace2d::particles::ParticleReferenceMemoryReport particleMemory = emitter.MemoryReport();

    trace2d::profile::StructuralProfileSnapshot2D structural{};
    const auto prepareStructural = structural.Prepare(
        trace2d::profile_adapters::StructuralProfileAdapterMetricCount2D);
    if (prepareStructural != trace2d::profile::StructuralProfileResult2D::Success)
    {
        throw std::runtime_error{
            "structural profile preparation failed: " +
            std::string{trace2d::profile::ToString(prepareStructural)}};
    }

    trace2d::profile_adapters::StructuralProfileInputs2D inputs{};
    inputs.renderer = renderer != nullptr ? &rendererMetrics : nullptr;
    inputs.resources = &resourceStats;
    inputs.resourceSnapshots = resourceSnapshots;
    inputs.resourceMemoryMeasured = true;
    inputs.particleReference = {
        .counters = &particleCounters,
        .memory = &particleMemory,
        .aliveCount = emitter.AliveCount(),
        .measured = true,
    };
    const auto composeResult = trace2d::profile_adapters::ComposeStructuralProfile2D(structural, inputs);
    if (composeResult != trace2d::profile::StructuralProfileResult2D::Success)
    {
        throw std::runtime_error{
            "structural profile composition failed: " +
            std::string{trace2d::profile::ToString(composeResult)}};
    }

    trace2d::profile::ProfileReportContext2D context{};
    context.engineVersion = trace2d::core::Version();
    context.sourceRevision = TRACE2D_PROFILE_SOURCE_REVISION;
    context.workload = WorkloadName;
    context.buildConfiguration = BuildConfiguration();
    context.operatingSystem = OperatingSystem();
    context.architecture = Architecture();
    context.compiler = Compiler();
    context.cpuIdentityAvailability = trace2d::profile::ProfileMetricAvailability2D::NotMeasured;
    context.rendererBackend = rendererBackend;
    context.timingSource = TimingSource;
    context.fixedTimestepNanoseconds = static_cast<std::uint64_t>(runtime.Config().fixedTimestep.count());
    context.warmupFrameCount = options.warmupFrameCount;
    context.requestedSampleFrameCount = options.sampleFrameCount;

    std::string report{};
    const auto reportResult = trace2d::profile::BuildProfileReportJson(
        structural,
        cpuProfiler,
        context,
        report);
    if (reportResult != trace2d::profile::ProfileReportResult2D::Success)
    {
        throw std::runtime_error{
            "profile report build failed: " +
            std::string{trace2d::profile::ToString(reportResult)}};
    }

    if (renderer != nullptr)
    {
        renderer->DestroyTexture(renderTexture);
        const auto residencyResult = resources.SetTextureRendererResidency(publishedTexture.handle, false, 0U);
        if (!residencyResult.Succeeded())
        {
            throw std::runtime_error{"representative profile texture residency cleanup failed"};
        }
    }
    const auto unloadResult = resources.Unload(publishedTexture.handle.Untyped());
    if (!unloadResult.Succeeded())
    {
        throw std::runtime_error{"representative profile texture unload failed"};
    }

    return report;
}
} // namespace

int main(const int argc, char* argv[])
{
    Options options{};
    try
    {
        if (!ParseOptions(argc, argv, options))
        {
            return ExitSuccess;
        }

        const std::string report = RunProfile(options);
        if (!options.outputPath.empty())
        {
            WriteFile(options.outputPath, report);
        }

        if (options.json)
        {
            std::cout << report << '\n';
        }
        else
        {
            std::cout
                << "Trace2D representative profile complete\n"
                << "  mode: " << trace2d::platform::ToString(options.mode) << '\n'
                << "  warmup frames: " << options.warmupFrameCount << '\n'
                << "  sample frames: " << options.sampleFrameCount << '\n'
                << "  output: " << (options.outputPath.empty() ? "stdout disabled (use --json or --output)" : options.outputPath)
                << '\n'
                << "  timing policy: evidence only; no portable wall-clock threshold\n";
        }
        return ExitSuccess;
    }
    catch (const std::invalid_argument& error)
    {
        std::cerr << "trace2d-profile usage error: " << error.what() << "\n\n";
        PrintHelp();
        return ExitUsage;
    }
    catch (const std::exception& error)
    {
        std::cerr << "trace2d-profile failed: " << error.what() << '\n';
        return ExitRuntimeFailure;
    }
}
