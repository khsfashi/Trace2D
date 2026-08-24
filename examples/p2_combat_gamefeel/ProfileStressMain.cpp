#include "CombatGame.hpp"
#include "P2GeneratedAssets.hpp"

#include <trace2d/application/Application.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/core/Version.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/physics/PhysicsComponents2D.hpp>
#include <trace2d/profile/Profile2D.hpp>
#include <trace2d/profile/ProfileReport2D.hpp>
#include <trace2d/profile/StructuralProfile2D.hpp>
#include <trace2d/profile_adapters/ProfileAdapters2D.hpp>

#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
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

constexpr std::uint64_t SampleFrameCount = 90U;
constexpr std::uint64_t ScenarioSeed = 329U;
constexpr std::string_view WorkloadName = "trace2d.p2-cross-platform-stress.v1";
constexpr std::string_view TimingSource = "std::chrono::steady_clock";

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

trace2d::application::ApplicationConfig MakeConfig()
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

void RequireProfileResult(const trace2d::profile::ProfileResult2D result, const std::string_view operation)
{
    if (result != trace2d::profile::ProfileResult2D::Success)
    {
        throw std::runtime_error{
            std::string{operation} + " failed: " + std::string{trace2d::profile::ToString(result)}};
    }
}

void ValidateScenario(const CombatGame& game)
{
    const auto physics = game.PhysicsMetrics();
    const auto audio = game.AudioMetrics();
    const auto resources = game.ResourceStats();

    if (game.EnemyHealth() != CombatGame::MaximumEnemyHealth - 1U || game.AcceptedHitCount() != 1U)
    {
        throw std::runtime_error{"XSTRESS1 authoritative combat result changed"};
    }
    if (game.AcceptedSwingCount() != 1U || game.SemanticStartedEventCount() != 1U)
    {
        throw std::runtime_error{"XSTRESS1 cooldown/audio event result changed"};
    }
    if (physics.attachedBodyCount != 7U || physics.fixedStepCount != SampleFrameCount)
    {
        throw std::runtime_error{"XSTRESS1 Physics2D structural result changed"};
    }
    if (physics.overlapQueryCount != 1U || physics.overlapCapacityFailureCount != 0U ||
        physics.bodyCommandFailureCount != 0U)
    {
        throw std::runtime_error{"XSTRESS1 bounded Physics2D query/command result changed"};
    }
    if (audio.commandFailureCount != 0U || resources.readyResources < 1U || resources.filesystemQueries != 0U)
    {
        throw std::runtime_error{"XSTRESS1 audio/resource hot-path result changed"};
    }
}

[[nodiscard]] std::string RunStressProfile()
{
    trace2d::examples::EnsureP2GeneratedHitSfx(TRACE2D_P2_RUNTIME_DIR);

    RegistryTypes types{};
    CombatGame game{types.physics, types.audio, TRACE2D_P2_RUNTIME_DIR};
    const auto config = MakeConfig();
    trace2d::application::Application application{game, types.registry, config};
    ScheduleScenario(application);

    trace2d::profile::CpuProfiler2D cpuProfiler{};
    RequireProfileResult(cpuProfiler.Prepare(1U, static_cast<std::size_t>(SampleFrameCount), 1U), "CPU profiler Prepare");
    trace2d::profile::ProfileScopeId2D frameScope{};
    RequireProfileResult(cpuProfiler.RegisterScope("p2.combat.fixed_frame", frameScope), "CPU scope registration");
    cpuProfiler.SetEnabled(true);

    application.Start();
    for (std::uint64_t frame = 0U; frame < SampleFrameCount; ++frame)
    {
        const auto begin = trace2d::profile::SteadyProfileTimestamp2D();
        RequireProfileResult(cpuProfiler.BeginFrame(frame + 1U, begin), "CPU frame begin");
        RequireProfileResult(cpuProfiler.EnterScope(frameScope, begin), "CPU scope enter");
        application.StepFrames(1U);
        const auto end = trace2d::profile::SteadyProfileTimestamp2D();
        RequireProfileResult(cpuProfiler.ExitScope(frameScope, end), "CPU scope exit");
        RequireProfileResult(cpuProfiler.EndFrame(end), "CPU frame end");
    }

    ValidateScenario(game);

    const auto physics = game.PhysicsMetrics();
    const auto audio = game.AudioMetrics();
    const auto resources = game.ResourceStats();
    const std::vector<trace2d::assets::ResourceSnapshot> resourceSnapshots = game.Resources().InspectAll();

    trace2d::profile::StructuralProfileSnapshot2D structural{};
    const auto prepareResult = structural.Prepare(trace2d::profile_adapters::StructuralProfileAdapterMetricCount2D);
    if (prepareResult != trace2d::profile::StructuralProfileResult2D::Success)
    {
        throw std::runtime_error{
            "structural profile preparation failed: " +
            std::string{trace2d::profile::ToString(prepareResult)}};
    }

    trace2d::profile_adapters::StructuralProfileInputs2D inputs{};
    inputs.physics = &physics;
    inputs.audio = &audio;
    inputs.resources = &resources;
    inputs.resourceSnapshots = resourceSnapshots;
    inputs.resourceMemoryMeasured = true;
    const auto composeResult = trace2d::profile_adapters::ComposeStructuralProfile2D(structural, inputs);
    if (composeResult != trace2d::profile::StructuralProfileResult2D::Success)
    {
        throw std::runtime_error{
            "structural profile composition failed: " +
            std::string{trace2d::profile::ToString(composeResult)}};
    }

    trace2d::profile::ProfileReportContext2D context{};
    context.engineVersion = trace2d::core::Version();
    context.sourceRevision = TRACE2D_XSTRESS_SOURCE_REVISION;
    context.workload = WorkloadName;
    context.buildConfiguration = BuildConfiguration();
    context.operatingSystem = OperatingSystem();
    context.architecture = Architecture();
    context.compiler = Compiler();
    context.cpuIdentityAvailability = trace2d::profile::ProfileMetricAvailability2D::NotMeasured;
    context.rendererBackend = "headless";
    context.timingSource = TimingSource;
    context.fixedTimestepNanoseconds = static_cast<std::uint64_t>(config.runtime.fixedTimestep.count());
    context.warmupFrameCount = 0U;
    context.requestedSampleFrameCount = SampleFrameCount;

    std::string report{};
    const auto reportResult = trace2d::profile::BuildProfileReportJson(structural, cpuProfiler, context, report);
    if (reportResult != trace2d::profile::ProfileReportResult2D::Success)
    {
        throw std::runtime_error{
            "profile report build failed: " +
            std::string{trace2d::profile::ToString(reportResult)}};
    }

    application.Stop();
    return report;
}

void WriteReport(const std::string& path, const std::string& report)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        throw std::runtime_error{"could not open XSTRESS1 profile output: " + path};
    }
    output.write(report.data(), static_cast<std::streamsize>(report.size()));
    output.put('\n');
    if (!output)
    {
        throw std::runtime_error{"could not write XSTRESS1 profile output: " + path};
    }
}
} // namespace

int main(const int argc, char* argv[])
{
    try
    {
        if (argc > 2)
        {
            std::cerr << "usage: trace2d_p2_combat_gamefeel_profile [output.json]\n";
            return 2;
        }

        const std::string report = RunStressProfile();
        if (argc == 2)
        {
            WriteReport(argv[1], report);
            std::cout << "XSTRESS1 profile PASS: output=" << argv[1] << '\n';
        }
        else
        {
            std::cout << report << '\n';
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Trace2D XSTRESS1 P2 profile failed: " << error.what() << '\n';
        return 30;
    }
}
