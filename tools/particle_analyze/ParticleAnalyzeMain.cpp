#include <trace2d/particles/ParticleProgram.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef TRACE2D_BUILD_CONFIGURATION
#define TRACE2D_BUILD_CONFIGURATION "unknown"
#endif

#define TRACE2D_STRINGIFY_IMPL(value) #value
#define TRACE2D_STRINGIFY(value) TRACE2D_STRINGIFY_IMPL(value)

namespace
{
struct Options final
{
    bool help{false};
    bool timing{false};
    std::string projectRoot{};
    std::string effectReference{};
    std::uint32_t frames{120U};
    std::uint64_t seed{1U};
    std::uint64_t stableId{1U};
    std::uint32_t warmupIterations{10U};
    std::uint32_t timingIterations{50U};
    std::string machineLabel{};
    std::string cpuModel{};
};

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

[[nodiscard]] bool TryParseU64(const std::string_view text, std::uint64_t& value) noexcept
{
    std::uint64_t parsed = 0U;
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool TryParsePositiveU32(const std::string_view text, std::uint32_t& value) noexcept
{
    std::uint64_t parsed = 0U;
    if (!TryParseU64(text, parsed) || parsed == 0U ||
        parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool TryParseNonNegativeU32(const std::string_view text, std::uint32_t& value) noexcept
{
    std::uint64_t parsed = 0U;
    if (!TryParseU64(text, parsed) || parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] Options ParseOptions(const int argc, char** const argv)
{
    Options options{};
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h")
        {
            options.help = true;
            continue;
        }
        if (argument == "--timing")
        {
            options.timing = true;
            continue;
        }

        const auto requireValue = [&](const char* const optionName) -> std::string_view
        {
            if (index + 1 >= argc)
            {
                throw std::invalid_argument{std::string{optionName} + " requires a value."};
            }
            ++index;
            return std::string_view{argv[index]};
        };

        if (argument == "--project-root")
        {
            options.projectRoot = requireValue("--project-root");
        }
        else if (argument == "--effect")
        {
            options.effectReference = requireValue("--effect");
        }
        else if (argument == "--frames")
        {
            const std::string_view value = requireValue("--frames");
            if (!TryParsePositiveU32(value, options.frames))
            {
                throw std::invalid_argument{"--frames must be a positive 32-bit integer."};
            }
        }
        else if (argument == "--seed")
        {
            const std::string_view value = requireValue("--seed");
            if (!TryParseU64(value, options.seed))
            {
                throw std::invalid_argument{"--seed must be a non-negative 64-bit integer."};
            }
        }
        else if (argument == "--stable-id")
        {
            const std::string_view value = requireValue("--stable-id");
            if (!TryParseU64(value, options.stableId))
            {
                throw std::invalid_argument{"--stable-id must be a non-negative 64-bit integer."};
            }
        }
        else if (argument == "--warmup")
        {
            const std::string_view value = requireValue("--warmup");
            if (!TryParseNonNegativeU32(value, options.warmupIterations))
            {
                throw std::invalid_argument{"--warmup must be a non-negative 32-bit integer."};
            }
        }
        else if (argument == "--iterations")
        {
            const std::string_view value = requireValue("--iterations");
            if (!TryParsePositiveU32(value, options.timingIterations))
            {
                throw std::invalid_argument{"--iterations must be a positive 32-bit integer."};
            }
        }
        else if (argument == "--machine-label")
        {
            options.machineLabel = requireValue("--machine-label");
        }
        else if (argument == "--cpu-model")
        {
            options.cpuModel = requireValue("--cpu-model");
        }
        else
        {
            throw std::invalid_argument{"Unknown particle analysis option: " + std::string{argument}};
        }
    }

    if (!options.help && (options.projectRoot.empty() || options.effectReference.empty()))
    {
        throw std::invalid_argument{"--project-root and --effect are required."};
    }
    if (options.timing && (options.machineLabel.empty() || options.cpuModel.empty()))
    {
        throw std::invalid_argument{"--timing requires --machine-label and --cpu-model metadata."};
    }
    return options;
}

void PrintHelp()
{
    std::cout
        << "trace2d_particle_analyze --project-root <path> --effect <project-relative-effect> [options]\n"
        << "\n"
        << "Deterministic structural analysis:\n"
        << "  --frames <N>       Exact playback workload frame count (default 120).\n"
        << "  --seed <N>         CPU reference seed (default 1).\n"
        << "  --stable-id <N>    Stable emitter ID (default 1).\n"
        << "\n"
        << "Optional machine-dependent timing (use a Release build):\n"
        << "  --timing\n"
        << "  --warmup <N>       Warmup workload iterations (default 10, zero allowed).\n"
        << "  --iterations <N>   Measured workload iterations (default 50).\n"
        << "  --machine-label <text>\n"
        << "  --cpu-model <text>\n";
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

void WriteFeatures(std::ostream& output, const trace2d::particles::ParticleProgram& program)
{
    output << '[';
    bool first = true;
    for (std::uint8_t index = 0U;
         index <= static_cast<std::uint8_t>(trace2d::particles::ParticleProgramFeature::AdditiveBlend);
         ++index)
    {
        const auto feature = static_cast<trace2d::particles::ParticleProgramFeature>(index);
        if (!trace2d::particles::HasParticleProgramFeature(program.featureMask, feature))
        {
            continue;
        }
        if (!first) output << ',';
        first = false;
        WriteJsonString(output, trace2d::particles::ToString(feature));
    }
    output << ']';
}

void WriteAttributes(
    std::ostream& output,
    const trace2d::particles::ParticleProgramMask mask)
{
    output << '[';
    bool first = true;
    for (std::uint8_t index = 0U;
         index <= static_cast<std::uint8_t>(trace2d::particles::ParticleProgramAttribute::SpriteIndex);
         ++index)
    {
        const auto attribute = static_cast<trace2d::particles::ParticleProgramAttribute>(index);
        if (!trace2d::particles::HasParticleProgramAttribute(mask, attribute))
        {
            continue;
        }
        if (!first) output << ',';
        first = false;
        WriteJsonString(output, trace2d::particles::ToString(attribute));
    }
    output << ']';
}

void WriteRandomChannels(std::ostream& output, const trace2d::particles::ParticleProgram& program)
{
    output << '[';
    for (std::uint32_t index = 0U; index < program.requiredRandomChannelCount; ++index)
    {
        if (index != 0U) output << ',';
        WriteJsonString(output, trace2d::particles::ToString(program.requiredRandomChannels[index]));
    }
    output << ']';
}

void WriteGpuLayout(std::ostream& output, const trace2d::particles::ParticleProgram& program)
{
    output << "{\"stride_bytes\":" << program.gpuStrideBytes;
    output << ",\"buffer_bytes_per_emitter\":" << program.gpuBufferBytes;
    output << ",\"pipeline_variant_id\":" << program.gpuPipelineVariantId;
    output << ",\"fields\":[";
    for (std::uint32_t index = 0U; index < program.gpuFieldCount; ++index)
    {
        if (index != 0U) output << ',';
        const auto& field = program.gpuFields[index];
        output << "{\"name\":";
        WriteJsonString(output, trace2d::particles::ToString(field.kind));
        output << ",\"offset_bytes\":" << field.offsetBytes;
        output << ",\"size_bytes\":" << field.sizeBytes << '}';
    }
    output << "]}";
}

void WriteOperationTotals(
    std::ostream& output,
    const trace2d::particles::ParticleProgram& program,
    const trace2d::particles::ParticleStructuralCostReport& report)
{
    output << '[';
    for (std::size_t index = 0U; index < report.operationTotals.size(); ++index)
    {
        if (index != 0U) output << ',';
        const auto& cost = program.operationCosts[index];
        const auto& total = report.operationTotals[index];
        output << "{\"operation\":";
        WriteJsonString(output, trace2d::particles::ToString(total.operation));
        output << ",\"per_admitted_spawn\":" << cost.perAdmittedSpawn;
        output << ",\"per_updated_particle\":" << cost.perUpdatedParticle;
        output << ",\"per_surviving_updated_particle\":" << cost.perSurvivingUpdatedParticle;
        output << ",\"evaluations\":" << total.evaluations << '}';
    }
    output << ']';
}

void StepPlayback(
    trace2d::particles::ParticleEmitter2D& emitter,
    const std::uint32_t frames)
{
    for (std::uint32_t frame = 0U; frame < frames; ++frame)
    {
        if (!emitter.Step())
        {
            throw std::runtime_error{"CPU reference workload step failed."};
        }
    }
}

[[nodiscard]] trace2d::particles::ParticleStructuralCostReport RunStructuralWorkload(
    const trace2d::particles::ParticleProgram& program,
    const Options& options)
{
    trace2d::particles::ParticleEmitter2D emitter{};
    const auto prepare = trace2d::particles::PrepareParticleProgramCpuEmitter(
        program,
        options.seed,
        options.stableId,
        emitter);
    if (!prepare.Ok())
    {
        throw std::runtime_error{"Failed to prepare CPU reference workload for particle analysis."};
    }

    emitter.Restart();
    trace2d::particles::ParticleCostAccumulator accumulator{};
    accumulator.Reset(emitter);
    for (std::uint32_t frame = 0U; frame < options.frames; ++frame)
    {
        if (!emitter.Step())
        {
            throw std::runtime_error{"CPU reference workload step failed."};
        }
        accumulator.ObserveAfterStep(emitter);
    }
    return trace2d::particles::BuildParticleStructuralCostReport(
        program,
        accumulator.Observation(emitter));
}

void WriteTiming(
    std::ostream& output,
    const trace2d::particles::ParticleProgram& program,
    const trace2d::particles::ParticleStructuralCostReport& structural,
    const Options& options)
{
    trace2d::particles::ParticleEmitter2D emitter{};
    const auto prepare = trace2d::particles::PrepareParticleProgramCpuEmitter(
        program,
        options.seed,
        options.stableId,
        emitter);
    if (!prepare.Ok())
    {
        throw std::runtime_error{"Failed to prepare CPU reference timing workload."};
    }

    for (std::uint32_t iteration = 0U; iteration < options.warmupIterations; ++iteration)
    {
        emitter.Restart();
        StepPlayback(emitter, options.frames);
    }

    std::vector<std::uint64_t> samples{};
    samples.reserve(options.timingIterations);
    std::uint64_t totalNanoseconds = 0U;
    for (std::uint32_t iteration = 0U; iteration < options.timingIterations; ++iteration)
    {
        // Reset/playback setup is deliberately outside the measured interval. The measured
        // scope is only repeated ParticleEmitter2D::Step() work for the deterministic window.
        emitter.Restart();
        const auto start = std::chrono::steady_clock::now();
        StepPlayback(emitter, options.frames);
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        const std::uint64_t nanoseconds = elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0U;
        samples.push_back(nanoseconds);
        totalNanoseconds += nanoseconds;
    }

    std::sort(samples.begin(), samples.end());
    const std::size_t medianIndex = samples.size() / 2U;
    const std::size_t p95Index = ((samples.size() * 95U + 99U) / 100U) - 1U;
    const double averageNanoseconds =
        static_cast<double>(totalNanoseconds) / static_cast<double>(samples.size());
    const std::uint64_t totalParticleUpdates =
        structural.particleUpdates * static_cast<std::uint64_t>(options.timingIterations);
    const double nanosecondsPerParticleUpdate = totalParticleUpdates == 0U
        ? 0.0
        : static_cast<double>(totalNanoseconds) / static_cast<double>(totalParticleUpdates);

    output << "{\"metric_source\":\"machine_dependent_timing\"";
    output << ",\"deterministic_equality_member\":false";
    output << ",\"scope\":\"particle_emitter_step_window\"";
    output << ",\"warmup_iterations\":" << options.warmupIterations;
    output << ",\"measured_iterations\":" << options.timingIterations;
    output << ",\"frames_per_iteration\":" << options.frames;
    output << ",\"average_ns\":" << averageNanoseconds;
    output << ",\"median_ns\":" << samples[medianIndex];
    output << ",\"p95_ns\":" << samples[p95Index];
    output << ",\"ns_per_particle_update\":" << nanosecondsPerParticleUpdate;
    output << ",\"environment\":{\"machine_label\":";
    WriteJsonString(output, options.machineLabel);
    output << ",\"cpu_model\":";
    WriteJsonString(output, options.cpuModel);
    output << ",\"logical_processors\":" << std::thread::hardware_concurrency();
    output << ",\"os\":";
    WriteJsonString(output, OperatingSystemName());
    output << ",\"compiler\":";
    WriteJsonString(output, CompilerId());
    output << ",\"compiler_version\":";
    WriteJsonString(output, CompilerVersion());
    output << ",\"build_configuration\":";
    WriteJsonString(output, TRACE2D_BUILD_CONFIGURATION);
    output << "}}";
}

void WriteResult(
    const trace2d::particles::ParticleProgram& program,
    const trace2d::particles::ParticleStructuralCostReport& report,
    const Options& options)
{
    std::cout << "{\"command\":\"particle-analyze\"";
    std::cout << ",\"metric_source\":\"deterministic_structure\"";
    std::cout << ",\"analysis_execution_backend\":\"cpu_reference_oracle\"";
    std::cout << ",\"effect_asset_id\":";
    WriteJsonString(std::cout, program.effectAssetId);
    std::cout << ",\"effect_semantic_id\":";
    WriteJsonString(std::cout, program.semanticId);
    std::cout << ",\"selected_backend\":";
    WriteJsonString(std::cout, trace2d::particles::ToString(program.selectedBackend));
    std::cout << ",\"program_fingerprint\":" << program.fingerprint;
    std::cout << ",\"features\":";
    WriteFeatures(std::cout, program);
    std::cout << ",\"required_random_channels\":";
    WriteRandomChannels(std::cout, program);
    std::cout << ",\"attributes\":{\"cpu_stored\":";
    WriteAttributes(std::cout, program.cpuStoredAttributeMask);
    std::cout << ",\"spawn_written\":";
    WriteAttributes(std::cout, program.spawnAttributeMask);
    std::cout << ",\"update_read\":";
    WriteAttributes(std::cout, program.updateReadAttributeMask);
    std::cout << ",\"update_written\":";
    WriteAttributes(std::cout, program.updateWriteAttributeMask);
    std::cout << ",\"render_only\":";
    WriteAttributes(std::cout, program.renderOnlyAttributeMask);
    std::cout << ",\"constant\":";
    WriteAttributes(std::cout, program.constantAttributeMask);
    std::cout << ",\"gpu_derived\":";
    WriteAttributes(std::cout, program.derivedGpuAttributeMask);
    std::cout << '}';

    std::cout << ",\"workload\":{\"emitter_count\":" << report.emitterCount;
    std::cout << ",\"observed_frames\":" << report.observedFrames;
    std::cout << ",\"capacity_per_emitter\":" << report.capacityPerEmitter;
    std::cout << ",\"current_alive\":" << report.currentAlive;
    std::cout << ",\"peak_alive\":" << report.peakAlive;
    std::cout << ",\"spawn_attempts\":" << report.counters.spawnAttempts;
    std::cout << ",\"spawned\":" << report.counters.spawned;
    std::cout << ",\"updated\":" << report.counters.updated;
    std::cout << ",\"expired\":" << report.counters.expired;
    std::cout << ",\"dropped\":" << report.counters.dropped << '}';

    std::cout << ",\"cpu_reference_memory\":{\"bytes_per_particle\":" << report.bytesPerParticlePayload;
    std::cout << ",\"particle_storage_bytes\":" << report.particleStorageBytes;
    std::cout << ",\"prepared_state_bytes\":" << report.preparedCpuStateBytes;
    std::cout << ",\"steady_state_step_allocations\":" << report.steadyStateSimulationAllocations << '}';

    std::cout << ",\"work_counts\":{\"spawn_random_evaluations\":" << report.spawnRandomEvaluations;
    std::cout << ",\"particle_updates\":" << report.particleUpdates;
    std::cout << ",\"surviving_particle_updates\":" << report.survivingParticleUpdates;
    std::cout << ",\"operations\":";
    WriteOperationTotals(std::cout, program, report);
    std::cout << '}';

    std::cout << ",\"planned_gpu_layout\":";
    WriteGpuLayout(std::cout, program);

    const auto gpuArtifact = trace2d::particles::CompileParticleGpuArtifact(program);
    std::cout << ",\"gpu_artifact\":{\"status\":";
    WriteJsonString(std::cout, gpuArtifact.Ok() ? "ok" : trace2d::particles::ToString(gpuArtifact.error));
    if (gpuArtifact.Ok())
    {
        std::cout << ",\"artifact_fingerprint\":" << gpuArtifact.artifact.artifactFingerprint;
        std::cout << ",\"runtime_status\":\"not_available_until_particle_stage_6\"";
    }
    else
    {
        std::cout << ",\"message\":";
        WriteJsonString(std::cout, gpuArtifact.message);
    }
    std::cout << '}';

    if (options.timing)
    {
        std::cout << ",\"timing\":";
        WriteTiming(std::cout, program, report, options);
    }
    std::cout << ",\"backend_changed_by_analyzer\":false";
    std::cout << ",\"status\":\"ok\"}\n";
}
} // namespace

int main(const int argc, char** const argv)
{
    std::cout.imbue(std::locale::classic());
    try
    {
        const Options options = ParseOptions(argc, argv);
        if (options.help)
        {
            PrintHelp();
            return 0;
        }

        trace2d::particles::ParticleEffectCache cache{std::filesystem::path{options.projectRoot}};
        const auto loaded = cache.Load(options.effectReference);
        if (!loaded.Succeeded() || loaded.asset == nullptr)
        {
            std::cerr << "Particle effect load failed.";
            for (const auto& diagnostic : loaded.diagnostics)
            {
                std::cerr << "\n  " << trace2d::particles::ToString(diagnostic.code)
                          << " " << diagnostic.path << ": " << diagnostic.message;
            }
            std::cerr << '\n';
            return 2;
        }

        const trace2d::particles::ParticleProgram program =
            trace2d::particles::CompileParticleProgram(*loaded.asset);
        const trace2d::particles::ParticleStructuralCostReport report =
            RunStructuralWorkload(program, options);
        WriteResult(program, report, options);
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "trace2d_particle_analyze: " << exception.what() << '\n';
        return 1;
    }
}
