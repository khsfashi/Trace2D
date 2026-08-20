#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::audio
{
namespace
{
class TemporaryOutputAudioRoot final
{
public:
    TemporaryOutputAudioRoot()
    {
        const testing::TestInfo* const info = testing::UnitTest::GetInstance()->current_test_info();
        const std::string testName = info == nullptr ? "unknown" : info->name();
        root_ = std::filesystem::temp_directory_path() / ("trace2d_audio4_" + testName);
        std::error_code ignored{};
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::create_directories(root_ / "audio", ignored);
    }

    ~TemporaryOutputAudioRoot()
    {
        std::error_code ignored{};
        std::filesystem::remove_all(root_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return root_;
    }

private:
    std::filesystem::path root_{};
};

void WriteOutputU16(std::ofstream& stream, const std::uint16_t value)
{
    const std::array<char, 2> bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU)};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteOutputU32(std::ofstream& stream, const std::uint32_t value)
{
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
        static_cast<char>((value >> 16U) & 0xFFU),
        static_cast<char>((value >> 24U) & 0xFFU)};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteOutputPcm16Wav(
    const std::filesystem::path& path,
    const std::uint32_t sampleRateHz,
    const std::uint16_t channelCount,
    const std::vector<std::int16_t>& interleavedSamples)
{
    ASSERT_NE(channelCount, 0U);
    ASSERT_EQ(interleavedSamples.size() % channelCount, 0U);
    const std::uint32_t dataBytes =
        static_cast<std::uint32_t>(interleavedSamples.size() * sizeof(std::int16_t));
    const std::uint32_t byteRate = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(sampleRateHz) * channelCount * sizeof(std::int16_t));
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(channelCount) * sizeof(std::int16_t));

    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(stream.is_open());
    stream.write("RIFF", 4);
    WriteOutputU32(stream, 36U + dataBytes);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    WriteOutputU32(stream, 16U);
    WriteOutputU16(stream, 1U);
    WriteOutputU16(stream, channelCount);
    WriteOutputU32(stream, sampleRateHz);
    WriteOutputU32(stream, byteRate);
    WriteOutputU16(stream, blockAlign);
    WriteOutputU16(stream, 16U);
    stream.write("data", 4);
    WriteOutputU32(stream, dataBytes);
    for (const std::int16_t sample : interleavedSamples)
    {
        WriteOutputU16(stream, static_cast<std::uint16_t>(sample));
    }
    stream.flush();
    ASSERT_TRUE(stream.good());
}

[[nodiscard]] assets::AudioClipResource MakeOutputClip(
    const std::filesystem::path& path,
    const assets::AudioClipLoadPolicy policy,
    const std::uint64_t frameCount)
{
    assets::AudioClipResource clip{};
    clip.loadPolicy = policy;
    clip.sampleRateHz = 48000U;
    clip.channelCount = 2U;
    clip.frameCount = frameCount;
    clip.encodedByteSize = static_cast<std::uint64_t>(std::filesystem::file_size(path));
    return clip;
}

struct OutputFixture final
{
    scene::ComponentRegistry registry{};
    AudioComponentTypes2D types{};
    scene::Scene scene;
    assets::ResourceRegistry resources;

    explicit OutputFixture(const std::filesystem::path& root)
        : types{RegisterAudio2DComponents(registry)}
        , scene{FreezeAndReturnRegistry()}
        , resources{root}
    {
    }

    [[nodiscard]] scene::EntityId AddSource(
        std::string semanticId,
        std::string clipReference,
        const AudioGroup2D group = AudioGroup2D::Sfx)
    {
        scene::EntityDescriptor descriptor{};
        descriptor.semanticId = std::move(semanticId);
        const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));
        AudioSource2D source{};
        source.clipReference = std::move(clipReference);
        source.group = group;
        (void)scene.AddComponent(entity, types.source, std::move(source));
        return entity;
    }

private:
    [[nodiscard]] scene::ComponentRegistry& FreezeAndReturnRegistry()
    {
        registry.Freeze();
        return registry;
    }
};

[[nodiscard]] AudioOutputConfig2D SmallOutputConfig()
{
    AudioOutputConfig2D config{};
    config.voiceCapacity = 4U;
    config.preloadCacheCapacity = 2U;
    config.refillChunkFrames = 4U;
    config.targetQueuedFrames = 8U;
    config.maxRefillChunksPerPump = 2U;
    return config;
}
} // namespace

TEST(AudioOutput2D, RejectsInvalidBoundsBeforeOpeningBackend)
{
    assets::ResourceRegistry resources{"."};
    AudioOutputConfig2D config = SmallOutputConfig();
    config.voiceCapacity = 0U;
    AudioOutput2D output{resources, config};

    EXPECT_EQ(output.Start(), AudioOutputResult2D::InvalidConfig);
    EXPECT_EQ(output.State(), AudioOutputState2D::Stopped);
    EXPECT_EQ(output.Metrics().deviceOpenCount, 0U);
}

TEST(AudioOutput2D, MirrorsSemanticLifecycleBeforeAnyPhysicalDeviceExists)
{
    TemporaryOutputAudioRoot temporary{};
    const std::filesystem::path sourcePath = temporary.Path() / "audio" / "mirror.wav";
    WriteOutputPcm16Wav(sourcePath, 48000U, 2U, {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000});

    OutputFixture fixture{temporary.Path()};
    ASSERT_TRUE(fixture.resources.PublishAudioClip(
        "audio/mirror.wav",
        MakeOutputClip(sourcePath, assets::AudioClipLoadPolicy::Preload, 4U)).Succeeded());
    const scene::EntityId entity = fixture.AddSource("mirror", "audio/mirror.wav");

    AudioSystem2D semantic{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(semantic.ReserveVoices(2U));
    ASSERT_TRUE(semantic.ReserveEvents(16U));
    AudioOutput2D output{fixture.resources, SmallOutputConfig()};

    const AudioPlayResult2D play = semantic.Play(entity);
    ASSERT_EQ(play.result, AudioCommandResult2D::Success);
    AudioOutputSyncReport2D sync = output.Sync(semantic, semantic.Events());
    EXPECT_EQ(sync.result, AudioOutputResult2D::Success);
    EXPECT_EQ(sync.createdVoiceCount, 1U);
    EXPECT_EQ(output.Metrics().trackedVoiceCount, 1U);
    EXPECT_EQ(output.Metrics().activeStreamCount, 0U);

    semantic.ClearEvents();
    ASSERT_EQ(semantic.Pause(play.voice), AudioCommandResult2D::Success);
    sync = output.Sync(semantic, semantic.Events());
    EXPECT_EQ(sync.result, AudioOutputResult2D::Success);
    EXPECT_EQ(output.Metrics().trackedVoiceCount, 1U);

    semantic.ClearEvents();
    ASSERT_EQ(semantic.Stop(play.voice), AudioCommandResult2D::Success);
    sync = output.Sync(semantic, semantic.Events());
    EXPECT_EQ(sync.result, AudioOutputResult2D::Success);
    EXPECT_EQ(sync.removedVoiceCount, 1U);
    EXPECT_EQ(output.Metrics().trackedVoiceCount, 0U);
}

TEST(AudioOutput2D, MirrorsStealEventBeforeReplacementGeneration)
{
    TemporaryOutputAudioRoot temporary{};
    const std::filesystem::path sourcePath = temporary.Path() / "audio" / "steal.wav";
    WriteOutputPcm16Wav(sourcePath, 48000U, 2U, {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000});

    OutputFixture fixture{temporary.Path()};
    ASSERT_TRUE(fixture.resources.PublishAudioClip(
        "audio/steal.wav",
        MakeOutputClip(sourcePath, assets::AudioClipLoadPolicy::Preload, 4U)).Succeeded());
    const scene::EntityId firstEntity = fixture.AddSource("first", "audio/steal.wav");
    const scene::EntityId secondEntity = fixture.AddSource("second", "audio/steal.wav");

    AudioSystem2D semantic{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(semantic.ReserveVoices(1U));
    ASSERT_TRUE(semantic.ReserveEvents(16U));
    AudioVoiceLimits2D limits{};
    limits.globalLimit = 1U;
    limits.overflowPolicy = AudioVoiceOverflowPolicy2D::StealOldest;
    ASSERT_TRUE(semantic.SetVoiceLimits(limits));

    AudioOutput2D output{fixture.resources, SmallOutputConfig()};
    const AudioPlayResult2D first = semantic.Play(firstEntity);
    ASSERT_EQ(first.result, AudioCommandResult2D::Success);
    ASSERT_EQ(output.Sync(semantic, semantic.Events()).result, AudioOutputResult2D::Success);
    semantic.ClearEvents();

    const AudioPlayResult2D second = semantic.Play(secondEntity);
    ASSERT_EQ(second.result, AudioCommandResult2D::Success);
    ASSERT_NE(first.voice, second.voice);
    const AudioOutputSyncReport2D sync = output.Sync(semantic, semantic.Events());
    EXPECT_EQ(sync.result, AudioOutputResult2D::Success);
    EXPECT_EQ(sync.removedVoiceCount, 1U);
    EXPECT_EQ(sync.createdVoiceCount, 1U);
    EXPECT_EQ(output.Metrics().trackedVoiceCount, 1U);
}

TEST(AudioOutput2D, DummyDevicePreloadReusesBoundedPreparedPcm)
{
    TemporaryOutputAudioRoot temporary{};
    const std::filesystem::path sourcePath = temporary.Path() / "audio" / "preload.wav";
    std::vector<std::int16_t> samples{};
    samples.reserve(64U);
    for (std::int16_t index = 0; index < 32; ++index)
    {
        samples.push_back(static_cast<std::int16_t>(index * 100));
        samples.push_back(static_cast<std::int16_t>(-index * 100));
    }
    WriteOutputPcm16Wav(sourcePath, 48000U, 2U, samples);

    OutputFixture fixture{temporary.Path()};
    ASSERT_TRUE(fixture.resources.PublishAudioClip(
        "audio/preload.wav",
        MakeOutputClip(sourcePath, assets::AudioClipLoadPolicy::Preload, 32U)).Succeeded());
    const scene::EntityId firstEntity = fixture.AddSource("first", "audio/preload.wav");
    const scene::EntityId secondEntity = fixture.AddSource("second", "audio/preload.wav");

    AudioSystem2D semantic{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(semantic.ReserveVoices(2U));
    ASSERT_TRUE(semantic.ReserveEvents(16U));
    AudioOutput2D output{fixture.resources, SmallOutputConfig()};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();

    ASSERT_EQ(semantic.Play(firstEntity).result, AudioCommandResult2D::Success);
    ASSERT_EQ(output.Sync(semantic, semantic.Events()).result, AudioOutputResult2D::Success);
    AudioOutputPumpReport2D pump = output.Pump();
    ASSERT_EQ(pump.result, AudioOutputResult2D::Success) << output.LastDiagnostic();
    EXPECT_EQ(pump.preparedVoiceCount, 1U);
    EXPECT_EQ(pump.refillChunkCount, 2U);
    EXPECT_EQ(pump.refillFrameCount, 8U);

    AudioOutputMetrics2D metrics = output.Metrics();
    EXPECT_EQ(metrics.activeStreamCount, 1U);
    EXPECT_EQ(metrics.preloadCacheEntryCount, 1U);
    EXPECT_EQ(metrics.trace2dOwnedPreloadPcmBytes, 32U * 2U * sizeof(float));
    EXPECT_GE(metrics.queuedInputBytes, 8U * 2U * sizeof(float));

    semantic.ClearEvents();
    ASSERT_EQ(semantic.Play(secondEntity).result, AudioCommandResult2D::Success);
    ASSERT_EQ(output.Sync(semantic, semantic.Events()).result, AudioOutputResult2D::Success);
    pump = output.Pump();
    ASSERT_EQ(pump.result, AudioOutputResult2D::Success) << output.LastDiagnostic();
    metrics = output.Metrics();
    EXPECT_EQ(metrics.trackedVoiceCount, 2U);
    EXPECT_EQ(metrics.activeStreamCount, 2U);
    EXPECT_EQ(metrics.preloadCacheEntryCount, 1U);
    EXPECT_EQ(metrics.trace2dOwnedPreloadPcmBytes, 32U * 2U * sizeof(float));

    output.Stop();
    EXPECT_EQ(output.State(), AudioOutputState2D::Stopped);
}

TEST(AudioOutput2D, DummyDeviceStreamingUsesRetainedBoundedRefillStorage)
{
    TemporaryOutputAudioRoot temporary{};
    const std::filesystem::path sourcePath = temporary.Path() / "audio" / "stream.wav";
    std::vector<std::int16_t> samples{};
    samples.reserve(64U);
    for (std::int16_t index = 0; index < 32; ++index)
    {
        samples.push_back(static_cast<std::int16_t>(index * 50));
        samples.push_back(static_cast<std::int16_t>(index * 25));
    }
    WriteOutputPcm16Wav(sourcePath, 48000U, 2U, samples);

    OutputFixture fixture{temporary.Path()};
    ASSERT_TRUE(fixture.resources.PublishAudioClip(
        "audio/stream.wav",
        MakeOutputClip(sourcePath, assets::AudioClipLoadPolicy::Stream, 32U)).Succeeded());
    const scene::EntityId entity = fixture.AddSource("stream", "audio/stream.wav", AudioGroup2D::Music);

    AudioSystem2D semantic{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(semantic.ReserveVoices(1U));
    ASSERT_TRUE(semantic.ReserveEvents(8U));
    const AudioPlayResult2D play = semantic.Play(entity);
    ASSERT_EQ(play.result, AudioCommandResult2D::Success);

    const AudioOutputConfig2D config = SmallOutputConfig();
    AudioOutput2D output{fixture.resources, config};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();
    ASSERT_EQ(output.Sync(semantic, semantic.Events()).result, AudioOutputResult2D::Success);
    const AudioOutputPumpReport2D pump = output.Pump();
    ASSERT_EQ(pump.result, AudioOutputResult2D::Success) << output.LastDiagnostic();
    EXPECT_EQ(pump.preparedVoiceCount, 1U);
    EXPECT_EQ(pump.refillFrameCount, 8U);

    const AudioOutputMetrics2D metrics = output.Metrics();
    EXPECT_EQ(metrics.streamingVoiceCount, 1U);
    EXPECT_EQ(metrics.preloadCacheEntryCount, 0U);
    EXPECT_EQ(metrics.trace2dOwnedPreloadPcmBytes, 0U);
    EXPECT_EQ(metrics.trace2dOwnedRefillBytes, config.refillChunkFrames * 2U * sizeof(float));
    EXPECT_EQ(metrics.trace2dOwnedRefillCapacityBytes, config.refillChunkFrames * 2U * sizeof(float));

    ASSERT_EQ(output.Suspend(), AudioOutputResult2D::Success) << output.LastDiagnostic();
    EXPECT_EQ(output.State(), AudioOutputState2D::Suspended);
    ASSERT_EQ(output.Resume(), AudioOutputResult2D::Success) << output.LastDiagnostic();
    EXPECT_EQ(output.State(), AudioOutputState2D::Running);
}
} // namespace trace2d::audio
