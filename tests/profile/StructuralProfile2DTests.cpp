#include <trace2d/profile/StructuralProfile2D.hpp>
#include <trace2d/profile_adapters/ProfileAdapters2D.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

namespace
{
using trace2d::profile::ProfileMetricAvailability2D;
using trace2d::profile::StructuralProfileMetric2D;
using trace2d::profile::StructuralProfileMetricKind2D;
using trace2d::profile::StructuralProfileResult2D;
using trace2d::profile::StructuralProfileSnapshot2D;
using trace2d::profile_adapters::StructuralProfileAdapterMetricCount2D;
using trace2d::profile_adapters::StructuralProfileInputs2D;

[[nodiscard]] const StructuralProfileMetric2D* FindMetric(
    const StructuralProfileSnapshot2D& snapshot,
    const std::string_view name)
{
    for (const auto& metric : snapshot.Metrics())
    {
        if (metric.Name() == name)
        {
            return &metric;
        }
    }

    return nullptr;
}

TEST(StructuralProfile2DTests, PreparedStorageIsReusedAndDuplicateNamesFailClosed)
{
    StructuralProfileSnapshot2D snapshot{};
    EXPECT_EQ(snapshot.Prepare(2U), StructuralProfileResult2D::Success);
    EXPECT_EQ(snapshot.Prepare(2U), StructuralProfileResult2D::AlreadyPrepared);

    EXPECT_EQ(
        snapshot.AddMetric(
            "test.zero",
            "1",
            StructuralProfileMetricKind2D::Gauge,
            ProfileMetricAvailability2D::Available,
            0U),
        StructuralProfileResult2D::Success);
    EXPECT_EQ(
        snapshot.AddMetric(
            "test.zero",
            "1",
            StructuralProfileMetricKind2D::Counter,
            ProfileMetricAvailability2D::Available,
            1U),
        StructuralProfileResult2D::DuplicateMetricName);

    const auto beforeClear = snapshot.StorageMetrics();
    EXPECT_EQ(beforeClear.metricCount, 1U);
    EXPECT_EQ(beforeClear.retainedMetricCapacity, 2U);
    EXPECT_EQ(beforeClear.rejectedMetricCount, 1U);

    EXPECT_EQ(snapshot.Clear(), StructuralProfileResult2D::Success);
    EXPECT_EQ(
        snapshot.AddMetric(
            "test.next",
            "By",
            StructuralProfileMetricKind2D::Gauge,
            ProfileMetricAvailability2D::Available,
            4U),
        StructuralProfileResult2D::Success);

    const auto afterClear = snapshot.StorageMetrics();
    EXPECT_EQ(afterClear.metricCount, 1U);
    EXPECT_EQ(afterClear.retainedMetricCapacity, beforeClear.retainedMetricCapacity);
    EXPECT_EQ(afterClear.clearCount, 1U);
}

TEST(StructuralProfile2DTests, CompositionDistinguishesMeasuredZeroFromMissingSource)
{
    StructuralProfileSnapshot2D snapshot{};
    ASSERT_EQ(snapshot.Prepare(StructuralProfileAdapterMetricCount2D), StructuralProfileResult2D::Success);

    trace2d::render::RenderMetrics renderer{};
    StructuralProfileInputs2D inputs{};
    inputs.renderer = &renderer;

    ASSERT_EQ(
        trace2d::profile_adapters::ComposeStructuralProfile2D(snapshot, inputs),
        StructuralProfileResult2D::Success);
    ASSERT_EQ(snapshot.Metrics().size(), StructuralProfileAdapterMetricCount2D);

    const auto* measuredZero = FindMetric(snapshot, "render.frame.submitted");
    ASSERT_NE(measuredZero, nullptr);
    EXPECT_EQ(measuredZero->availability, ProfileMetricAvailability2D::Available);
    EXPECT_EQ(measuredZero->value, 0U);

    const auto* missing = FindMetric(snapshot, "physics.body.attached");
    ASSERT_NE(missing, nullptr);
    EXPECT_EQ(missing->availability, ProfileMetricAvailability2D::NotMeasured);
    EXPECT_EQ(missing->value, 0U);
}

TEST(StructuralProfile2DTests, CompositionMapsRepresentativeSubsystemAndMemoryEvidence)
{
    StructuralProfileSnapshot2D snapshot{};
    ASSERT_EQ(snapshot.Prepare(StructuralProfileAdapterMetricCount2D), StructuralProfileResult2D::Success);

    trace2d::render::RenderMetrics renderer{};
    renderer.drawCalls = 7U;
    renderer.spriteVertexCapacityBytes = 4096U;

    trace2d::physics::PhysicsMetrics2D physics{};
    physics.attachedBodyCount = 3U;
    physics.fixedStepCount = 42U;

    trace2d::audio::AudioMetrics2D audio{};
    audio.activeVoiceCount = 2U;
    audio.stolenVoiceCount = 1U;

    trace2d::audio::AudioOutputMetrics2D audioOutput{};
    audioOutput.trace2dOwnedPreloadPcmBytes = 2048U;
    audioOutput.deviceLossEventCount = 1U;

    trace2d::assets::ResourceRegistryStats resourceStats{};
    resourceStats.readyResources = 2U;
    std::vector<trace2d::assets::ResourceSnapshot> resources(2U);
    resources[0].memory.knownRetainedCpuBytes = 10U;
    resources[0].memory.retainedContainerCapacityBytes = 3U;
    resources[0].memory.knownRendererGpuBytes = 5U;
    resources[1].memory.knownRetainedCpuBytes = 20U;
    resources[1].memory.retainedContainerCapacityBytes = 4U;
    resources[1].memory.knownRendererGpuBytes = 6U;

    trace2d::particles::ParticleReferenceCounters particleCounters{};
    particleCounters.spawned = 8U;
    particleCounters.peakAlive = 4U;
    trace2d::particles::ParticleReferenceMemoryReport particleMemory{};
    particleMemory.capacity = 16U;
    particleMemory.particleStorageBytes = 1000U;

    StructuralProfileInputs2D inputs{};
    inputs.renderer = &renderer;
    inputs.physics = &physics;
    inputs.audio = &audio;
    inputs.audioOutput = &audioOutput;
    inputs.resources = &resourceStats;
    inputs.resourceSnapshots = resources;
    inputs.resourceMemoryMeasured = true;
    inputs.particleReference = {
        &particleCounters,
        &particleMemory,
        3U,
        true,
    };

    ASSERT_EQ(
        trace2d::profile_adapters::ComposeStructuralProfile2D(snapshot, inputs),
        StructuralProfileResult2D::Success);

    const auto expectAvailable = [&snapshot](const std::string_view name, const std::uint64_t value)
    {
        const auto* metric = FindMetric(snapshot, name);
        ASSERT_NE(metric, nullptr) << name;
        EXPECT_EQ(metric->availability, ProfileMetricAvailability2D::Available) << name;
        EXPECT_EQ(metric->value, value) << name;
    };

    expectAvailable("render.draw.count", 7U);
    expectAvailable("render.sprite.vertex.capacity_bytes", 4096U);
    expectAvailable("physics.body.attached", 3U);
    expectAvailable("physics.step.fixed", 42U);
    expectAvailable("audio.voice.active", 2U);
    expectAvailable("audio.voice.stolen", 1U);
    expectAvailable("audio.output.memory.preload.bytes", 2048U);
    expectAvailable("audio.output.device.loss", 1U);
    expectAvailable("resource.ready", 2U);
    expectAvailable("resource.memory.cpu.retained_bytes", 30U);
    expectAvailable("resource.memory.container.capacity_bytes", 7U);
    expectAvailable("resource.memory.renderer_gpu_bytes", 11U);
    expectAvailable("particle.reference.alive", 3U);
    expectAvailable("particle.reference.spawned", 8U);
    expectAvailable("particle.reference.capacity", 16U);
    expectAvailable("particle.reference.memory.particle_storage_bytes", 1000U);
}

TEST(StructuralProfile2DTests, ResourceByteOverflowPreservesPreviousSnapshot)
{
    StructuralProfileSnapshot2D snapshot{};
    ASSERT_EQ(snapshot.Prepare(StructuralProfileAdapterMetricCount2D), StructuralProfileResult2D::Success);

    StructuralProfileInputs2D initialInputs{};
    ASSERT_EQ(
        trace2d::profile_adapters::ComposeStructuralProfile2D(snapshot, initialInputs),
        StructuralProfileResult2D::Success);
    ASSERT_EQ(snapshot.Metrics().size(), StructuralProfileAdapterMetricCount2D);
    const std::string firstMetricName{snapshot.Metrics().front().Name()};

    trace2d::assets::ResourceRegistryStats stats{};
    std::vector<trace2d::assets::ResourceSnapshot> resources(2U);
    resources[0].memory.knownRetainedCpuBytes = std::numeric_limits<std::size_t>::max();
    resources[1].memory.knownRetainedCpuBytes = 1U;

    StructuralProfileInputs2D overflowInputs{};
    overflowInputs.resources = &stats;
    overflowInputs.resourceSnapshots = resources;
    overflowInputs.resourceMemoryMeasured = true;

    EXPECT_EQ(
        trace2d::profile_adapters::ComposeStructuralProfile2D(snapshot, overflowInputs),
        StructuralProfileResult2D::ValueOverflow);
    ASSERT_EQ(snapshot.Metrics().size(), StructuralProfileAdapterMetricCount2D);
    EXPECT_EQ(snapshot.Metrics().front().Name(), firstMetricName);
}

TEST(StructuralProfile2DTests, InsufficientCompositionCapacityDoesNotPublishPartialSnapshot)
{
    StructuralProfileSnapshot2D snapshot{};
    ASSERT_EQ(
        snapshot.Prepare(StructuralProfileAdapterMetricCount2D - 1U),
        StructuralProfileResult2D::Success);

    EXPECT_EQ(
        trace2d::profile_adapters::ComposeStructuralProfile2D(snapshot, {}),
        StructuralProfileResult2D::MetricCapacityExceeded);
    EXPECT_TRUE(snapshot.Metrics().empty());
    EXPECT_EQ(snapshot.StorageMetrics().clearCount, 0U);
}

TEST(StructuralProfile2DTests, JsonReportIsExplicitVersionedAndEscapesContext)
{
    StructuralProfileSnapshot2D snapshot{};
    ASSERT_EQ(snapshot.Prepare(1U), StructuralProfileResult2D::Success);
    ASSERT_EQ(
        snapshot.AddMetric(
            "test.value",
            "By",
            StructuralProfileMetricKind2D::Counter,
            ProfileMetricAvailability2D::Available,
            3U),
        StructuralProfileResult2D::Success);

    trace2d::profile::StructuralProfileReportContext2D context{};
    context.engineVersion = "0.1.0";
    context.sourceRevision = "abc123";
    context.workload = "quoted\"\nworkload";
    context.buildConfiguration = "Release";
    context.operatingSystem = "test-os";
    context.compiler = "test-compiler";
    context.rendererBackend = "test-backend";
    context.frameIndex = 9U;

    const auto json = trace2d::profile::BuildStructuralProfileJson(snapshot, context);
    EXPECT_NE(json.find("trace2d.profile.structural.v1"), std::string::npos);
    EXPECT_NE(json.find("\\\"\\nworkload"), std::string::npos);
    EXPECT_NE(json.find("\"kind\":\"counter\""), std::string::npos);
    EXPECT_NE(json.find("\"unit\":\"By\""), std::string::npos);
    EXPECT_NE(json.find("\"availability\":\"available\""), std::string::npos);
    EXPECT_NE(json.find("\"value\":3"), std::string::npos);
}
} // namespace
