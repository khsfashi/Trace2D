#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#ifndef TRACE2D_BUILD_CONFIGURATION
#define TRACE2D_BUILD_CONFIGURATION "unknown"
#endif

namespace
{
using namespace std::chrono_literals;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationEmission2D;
using trace2d::runtime::SpriteAnimationEmissionKind;
using trace2d::runtime::SpriteAnimationEvent2D;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimationSpeed2D;
using trace2d::runtime::SpriteAnimator2D;
using trace2d::runtime::SpriteAnimator2DState;

constexpr std::string_view kSchema = "trace2d.sprite-animation-workload.v1";
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

struct Options final
{
    bool list{false};
    bool timing{false};
    std::string workloadName{};
    std::uint32_t iterations{50};
    std::uint32_t warmupIterations{10};
    std::string machineLabel{};
};

struct WorkloadDefinition final
{
    std::string_view name{};
    std::array<std::chrono::nanoseconds, 8> frameDurations{};
    std::uint32_t frameCount{0};
    std::array<SpriteAnimationEvent2D, 16> events{};
    std::uint32_t eventCount{0};
    SpriteAnimationLoopMode loopMode{SpriteAnimationLoopMode::Loop};
    SpriteAnimationDirection direction{SpriteAnimationDirection::Forward};
    SpriteAnimationSpeed2D speed{};
    std::chrono::nanoseconds startTime{0};
    std::chrono::nanoseconds stepDelta{0};
    std::uint32_t steps{0};
};

struct WorkloadSummary final
{
    std::uint32_t frameCount{0};
    std::uint32_t eventCount{0};
    std::int64_t clipDurationNs{0};
    std::int64_t stepDeltaNs{0};
    std::uint32_t steps{0};
    std::uint64_t successfulAdvances{0};
    std::uint64_t authoredEvents{0};
    std::uint64_t loops{0};
    std::uint64_t bounces{0};
    std::uint64_t completions{0};
    std::int64_t finalTimeNs{0};
    std::uint32_t finalFrameIndex{0};
    SpriteAnimationDirection finalDirection{SpriteAnimationDirection::Forward};
    bool completed{false};
    std::uint32_t speedRemainder{0};
    std::uint64_t transcriptDigest{kFnvOffset};

    [[nodiscard]] bool operator==(const WorkloadSummary&) const noexcept = default;
};

struct Fixture final
{
    SpriteAsset asset{};
    SpriteAnimationClip2D clip{};
    SpriteAnimator2D animator{};
};

[[nodiscard]] constexpr std::array<WorkloadDefinition, 3> Workloads() noexcept
{
    return {{
        {
            "steady_loop_rational",
            {80ms, 120ms, 100ms, 0ns, 0ns, 0ns, 0ns, 0ns},
            3U,
            {{{101U, 0ms, 0U}, {102U, 50ms, 1U}, {103U, 200ms, 2U}}},
            3U,
            SpriteAnimationLoopMode::Loop,
            SpriteAnimationDirection::Forward,
            {2U, 3U},
            0ns,
            16'666'667ns,
            6'000U,
        },
        {
            "dense_event_ping_pong",
            {25ms, 25ms, 25ms, 25ms, 25ms, 25ms, 25ms, 25ms},
            8U,
            {{{201U, 10ms, 0U},
              {202U, 20ms, 1U},
              {203U, 30ms, 2U},
              {204U, 40ms, 3U},
              {205U, 50ms, 4U},
              {206U, 50ms, 5U},
              {207U, 75ms, 6U},
              {208U, 100ms, 7U},
              {209U, 125ms, 8U},
              {210U, 150ms, 9U},
              {211U, 175ms, 10U},
              {212U, 190ms, 11U}}},
            12U,
            SpriteAnimationLoopMode::PingPong,
            SpriteAnimationDirection::Forward,
            {5U, 4U},
            0ns,
            33'333'333ns,
            4'000U,
        },
        {
            "large_step_multi_wrap",
            {90ms, 110ms, 70ms, 130ms, 0ns, 0ns, 0ns, 0ns},
            4U,
            {{{301U, 0ms, 0U},
              {302U, 45ms, 1U},
              {303U, 90ms, 2U},
              {304U, 200ms, 3U},
              {305U, 270ms, 4U},
              {306U, 399ms, 5U}}},
            6U,
            SpriteAnimationLoopMode::Loop,
            SpriteAnimationDirection::Reverse,
            {1U, 1U},
            350ms,
            1'250ms,
            512U,
        },
    }};
}

[[nodiscard]] const WorkloadDefinition* FindWorkload(const std::string_view name) noexcept
{
    static constexpr auto workloads = Workloads();
    for (const auto& workload : workloads)
    {
        if (workload.name == name)
        {
            return &workload;
        }
    }
    return nullptr;
}

void HashByte(std::uint64_t& hash, const std::uint8_t value) noexcept
{
    hash ^= value;
    hash *= kFnvPrime;
}

template <typename Integer>
void HashInteger(std::uint64_t& hash, const Integer value) noexcept
{
    using Unsigned = std::make_unsigned_t<Integer>;
    std::uint64_t bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Unsigned); ++index)
    {
        HashByte(hash, static_cast<std::uint8_t>(bits & 0xffU));
        bits >>= 8U;
    }
}

void HashEmission(std::uint64_t& hash, const SpriteAnimationEmission2D& emission) noexcept
{
    HashInteger(hash, static_cast<std::uint8_t>(emission.kind));
    HashInteger(hash, emission.eventId);
    HashInteger(hash, emission.authoredOrdinal);
    HashInteger(hash, emission.time.count());
    HashInteger(hash, static_cast<std::uint8_t>(emission.direction));
}

void HashFinalState(std::uint64_t& hash, const SpriteAnimator2DState& state) noexcept
{
    HashInteger(hash, state.time.count());
    HashInteger(hash, state.frameIndex);
    HashInteger(hash, static_cast<std::uint8_t>(state.playback));
    HashInteger(hash, static_cast<std::uint8_t>(state.loopMode));
    HashInteger(hash, static_cast<std::uint8_t>(state.direction));
    HashInteger(hash, static_cast<std::uint8_t>(state.completed ? 1U : 0U));
    HashInteger(hash, state.speed.numerator);
    HashInteger(hash, state.speed.denominator);
    HashInteger(hash, state.speedRemainder);
}

void WriteJsonString(std::ostream& output, const std::string_view value)
{
    static constexpr char hexDigits[] = "0123456789abcdef";
    output << '"';
    for (const char character : value)
    {
        const auto byte = static_cast<unsigned char>(character);
        switch (character)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
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
            if (byte < 0x20U)
            {
                output << "\\u00" << hexDigits[(byte >> 4U) & 0x0fU] << hexDigits[byte & 0x0fU];
            }
            else
            {
                output << character;
            }
            break;
        }
    }
    output << '"';
}

[[nodiscard]] bool TryParseU32(
    const std::string_view text,
    const bool allowZero,
    std::uint32_t& outValue) noexcept
{
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        (!allowZero && parsed == 0U) || parsed > std::numeric_limits<std::uint32_t>::max())
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
            if (!TryParseU32(requireValue("--iterations"), false, options.iterations))
            {
                throw std::invalid_argument{"--iterations must be a positive 32-bit integer."};
            }
        }
        else if (argument == "--warmup")
        {
            if (!TryParseU32(requireValue("--warmup"), true, options.warmupIterations))
            {
                throw std::invalid_argument{"--warmup must be a non-negative 32-bit integer."};
            }
        }
        else if (argument == "--machine-label")
        {
            options.machineLabel = requireValue("--machine-label");
        }
        else
        {
            throw std::invalid_argument{"Unknown Sprite animation workload option: " + std::string{argument}};
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
    if (options.timing && options.machineLabel.empty())
    {
        throw std::invalid_argument{"--timing requires --machine-label metadata."};
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

[[nodiscard]] std::string_view LoopModeName(const SpriteAnimationLoopMode mode) noexcept
{
    switch (mode)
    {
    case SpriteAnimationLoopMode::Once:
        return "once";
    case SpriteAnimationLoopMode::Loop:
        return "loop";
    case SpriteAnimationLoopMode::PingPong:
        return "ping_pong";
    }
    return "unknown";
}

[[nodiscard]] std::string_view DirectionName(const SpriteAnimationDirection direction) noexcept
{
    return direction == SpriteAnimationDirection::Forward ? "forward" : "reverse";
}

void PrepareFixture(const WorkloadDefinition& definition, Fixture& fixture)
{
    fixture.asset.regions.resize(definition.frameCount);
    std::array<SpriteAnimationFrame2D, 8> frames{};
    for (std::uint32_t index = 0; index < definition.frameCount; ++index)
    {
        frames[index] = SpriteAnimationFrame2D{index, definition.frameDurations[index]};
    }

    const auto clipStatus = SpriteAnimationClip2D::Prepare(
        &fixture.asset,
        definition.frameCount,
        std::span<const SpriteAnimationFrame2D>{frames.data(), definition.frameCount},
        std::span<const SpriteAnimationEvent2D>{definition.events.data(), definition.eventCount},
        fixture.clip);
    if (!clipStatus.Succeeded())
    {
        throw std::runtime_error{"Failed to prepare Sprite animation workload clip."};
    }

    SpriteAnimator2DState state{};
    const auto stateStatus = MakeSpriteAnimator2DState(
        fixture.clip,
        definition.startTime,
        SpriteAnimationPlaybackState::Playing,
        definition.loopMode,
        definition.direction,
        false,
        definition.speed,
        state);
    if (!stateStatus.Succeeded() || !fixture.animator.RestoreState(state).Succeeded())
    {
        throw std::runtime_error{"Failed to prepare Sprite animation workload state."};
    }
}

[[nodiscard]] WorkloadSummary RunStructuralWorkload(const WorkloadDefinition& definition)
{
    Fixture fixture{};
    PrepareFixture(definition, fixture);

    WorkloadSummary summary{};
    summary.frameCount = definition.frameCount;
    summary.eventCount = definition.eventCount;
    summary.clipDurationNs = fixture.clip.Duration().count();
    summary.stepDeltaNs = definition.stepDelta.count();
    summary.steps = definition.steps;

    std::array<SpriteAnimationEmission2D, 128> emissions{};
    for (std::uint32_t step = 0; step < definition.steps; ++step)
    {
        const auto result = fixture.animator.Advance(definition.stepDelta, emissions);
        if (!result.Succeeded())
        {
            throw std::runtime_error{"Sprite animation workload advance failed."};
        }

        ++summary.successfulAdvances;
        for (std::size_t index = 0; index < result.emissionCount; ++index)
        {
            const auto& emission = emissions[index];
            HashEmission(summary.transcriptDigest, emission);
            switch (emission.kind)
            {
            case SpriteAnimationEmissionKind::AuthoredEvent:
                ++summary.authoredEvents;
                break;
            case SpriteAnimationEmissionKind::Loop:
                ++summary.loops;
                break;
            case SpriteAnimationEmissionKind::Bounce:
                ++summary.bounces;
                break;
            case SpriteAnimationEmissionKind::Completed:
                ++summary.completions;
                break;
            }
        }
    }

    const auto& finalState = fixture.animator.State();
    summary.finalTimeNs = finalState.time.count();
    summary.finalFrameIndex = finalState.frameIndex;
    summary.finalDirection = finalState.direction;
    summary.completed = finalState.completed;
    summary.speedRemainder = finalState.speedRemainder;
    HashFinalState(summary.transcriptDigest, finalState);
    return summary;
}

[[nodiscard]] double RunTimedWorkload(const WorkloadDefinition& definition)
{
    Fixture fixture{};
    PrepareFixture(definition, fixture);
    std::array<SpriteAnimationEmission2D, 128> emissions{};
    std::uint64_t consumedEmissions = 0U;

    const auto begin = std::chrono::steady_clock::now();
    for (std::uint32_t step = 0; step < definition.steps; ++step)
    {
        const auto result = fixture.animator.Advance(definition.stepDelta, emissions);
        if (!result.Succeeded())
        {
            throw std::runtime_error{"Timed Sprite animation workload advance failed."};
        }
        consumedEmissions += result.emissionCount;
    }
    const auto end = std::chrono::steady_clock::now();

    if (consumedEmissions == std::numeric_limits<std::uint64_t>::max())
    {
        std::cerr << "unreachable";
    }
    return std::chrono::duration<double, std::micro>{end - begin}.count();
}

[[nodiscard]] double Median(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    const std::size_t middle = samples.size() / 2U;
    if ((samples.size() & 1U) != 0U)
    {
        return samples[middle];
    }
    return (samples[middle - 1U] + samples[middle]) / 2.0;
}

[[nodiscard]] double Percentile95(std::vector<double> samples)
{
    std::sort(samples.begin(), samples.end());
    const double rank = std::ceil(0.95 * static_cast<double>(samples.size()));
    const std::size_t index = static_cast<std::size_t>(std::max(1.0, rank)) - 1U;
    return samples[index];
}

void WriteSummaryJson(
    const WorkloadDefinition& definition,
    const WorkloadSummary& summary,
    const bool deterministicReplay)
{
    std::cout << '{'
              << "\"schema\":\"" << kSchema << "\","
              << "\"status\":\"ok\","
              << "\"workload\":\"" << definition.name << "\","
              << "\"deterministic_replay\":" << (deterministicReplay ? "true" : "false") << ','
              << "\"frame_count\":" << summary.frameCount << ','
              << "\"event_count\":" << summary.eventCount << ','
              << "\"clip_duration_ns\":" << summary.clipDurationNs << ','
              << "\"loop_mode\":\"" << LoopModeName(definition.loopMode) << "\","
              << "\"initial_direction\":\"" << DirectionName(definition.direction) << "\","
              << "\"speed_numerator\":" << definition.speed.numerator << ','
              << "\"speed_denominator\":" << definition.speed.denominator << ','
              << "\"step_delta_ns\":" << summary.stepDeltaNs << ','
              << "\"steps\":" << summary.steps << ','
              << "\"successful_advances\":" << summary.successfulAdvances << ','
              << "\"authored_events\":" << summary.authoredEvents << ','
              << "\"loops\":" << summary.loops << ','
              << "\"bounces\":" << summary.bounces << ','
              << "\"completions\":" << summary.completions << ','
              << "\"final_time_ns\":" << summary.finalTimeNs << ','
              << "\"final_frame_index\":" << summary.finalFrameIndex << ','
              << "\"final_direction\":\"" << DirectionName(summary.finalDirection) << "\","
              << "\"completed\":" << (summary.completed ? "true" : "false") << ','
              << "\"speed_remainder\":" << summary.speedRemainder << ','
              << "\"transcript_digest_fnv1a64\":\"" << summary.transcriptDigest << "\""
              << "}\n";
}

void WriteTimingJson(
    const WorkloadDefinition& definition,
    const Options& options,
    const WorkloadSummary& summary)
{
    for (std::uint32_t index = 0; index < options.warmupIterations; ++index)
    {
        static_cast<void>(RunTimedWorkload(definition));
    }

    std::vector<double> samples{};
    samples.reserve(options.iterations);
    for (std::uint32_t index = 0; index < options.iterations; ++index)
    {
        samples.push_back(RunTimedWorkload(definition));
    }

    const double total = std::accumulate(samples.begin(), samples.end(), 0.0);
    const double median = Median(samples);
    const double p95 = Percentile95(samples);

    std::cout << '{'
              << "\"schema\":\"" << kSchema << "\","
              << "\"status\":\"ok\","
              << "\"timing_scope\":\"sprite_animator_advance_window\","
              << "\"workload\":\"" << definition.name << "\","
              << "\"machine_label\":";
    WriteJsonString(std::cout, options.machineLabel);
    std::cout << ','
              << "\"os\":\"" << OperatingSystemName() << "\","
              << "\"compiler\":\"" << CompilerId() << "\","
              << "\"build_configuration\":\"" << TRACE2D_BUILD_CONFIGURATION << "\","
              << "\"warmup_iterations\":" << options.warmupIterations << ','
              << "\"measured_iterations\":" << options.iterations << ','
              << "\"steps_per_iteration\":" << definition.steps << ','
              << "\"average_us_per_window\":" << (total / static_cast<double>(samples.size())) << ','
              << "\"median_us_per_window\":" << median << ','
              << "\"p95_nearest_rank_us_per_window\":" << p95 << ','
              << "\"structural_digest_fnv1a64\":\"" << summary.transcriptDigest << "\""
              << "}\n";
}

void WriteListJson()
{
    static constexpr auto workloads = Workloads();
    std::cout << "{\"schema\":\"" << kSchema << "\",\"status\":\"ok\",\"workloads\":[";
    for (std::size_t index = 0; index < workloads.size(); ++index)
    {
        if (index != 0U)
        {
            std::cout << ',';
        }
        std::cout << '"' << workloads[index].name << '"';
    }
    std::cout << "]}\n";
}
} // namespace

int main(const int argc, char** const argv)
{
    try
    {
        const Options options = ParseOptions(argc, argv);
        if (options.list)
        {
            WriteListJson();
            return 0;
        }

        const WorkloadDefinition* const workload = FindWorkload(options.workloadName);
        if (workload == nullptr)
        {
            throw std::invalid_argument{"Unknown Sprite animation workload: " + options.workloadName};
        }

        const WorkloadSummary first = RunStructuralWorkload(*workload);
        const WorkloadSummary second = RunStructuralWorkload(*workload);
        if (!(first == second))
        {
            std::cerr << "Sprite animation workload replay mismatch.\n";
            return 2;
        }

        if (options.timing)
        {
            WriteTimingJson(*workload, options, first);
        }
        else
        {
            WriteSummaryJson(*workload, first, true);
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
