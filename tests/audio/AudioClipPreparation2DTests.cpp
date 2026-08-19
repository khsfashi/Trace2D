#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioClipPreparation2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace trace2d::audio
{
namespace
{
class TemporaryAudioRoot final
{
public:
    TemporaryAudioRoot()
    {
        const testing::TestInfo* const info = testing::UnitTest::GetInstance()->current_test_info();
        const std::string testName = info == nullptr ? "unknown" : info->name();
        root_ = std::filesystem::temp_directory_path() / ("trace2d_audio2_" + testName);
        std::error_code ignored{};
        std::filesystem::remove_all(root_, ignored);
        std::filesystem::create_directories(root_ / "audio", ignored);
    }

    ~TemporaryAudioRoot()
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

void WriteU16(std::ofstream& stream, const std::uint16_t value)
{
    const std::array<char, 2> bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU)};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WriteU32(std::ofstream& stream, const std::uint32_t value)
{
    const std::array<char, 4> bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
        static_cast<char>((value >> 16U) & 0xFFU),
        static_cast<char>((value >> 24U) & 0xFFU)};
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void WritePcm16Wav(
    const std::filesystem::path& path,
    const std::uint32_t sampleRateHz,
    const std::uint16_t channelCount,
    const std::vector<std::int16_t>& interleavedSamples)
{
    ASSERT_NE(channelCount, 0U);
    ASSERT_EQ(interleavedSamples.size() % channelCount, 0U);
    const std::uint32_t dataBytes =
        static_cast<std::uint32_t>(interleavedSamples.size() * sizeof(std::int16_t));
    const std::uint32_t byteRate = sampleRateHz * channelCount * sizeof(std::int16_t);
    const std::uint16_t blockAlign = channelCount * sizeof(std::int16_t);

    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(stream.is_open());
    stream.write("RIFF", 4);
    WriteU32(stream, 36U + dataBytes);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    WriteU32(stream, 16U);
    WriteU16(stream, 1U);
    WriteU16(stream, channelCount);
    WriteU32(stream, sampleRateHz);
    WriteU32(stream, byteRate);
    WriteU16(stream, blockAlign);
    WriteU16(stream, 16U);
    stream.write("data", 4);
    WriteU32(stream, dataBytes);
    for (const std::int16_t sample : interleavedSamples)
    {
        WriteU16(stream, static_cast<std::uint16_t>(sample));
    }
    stream.flush();
    ASSERT_TRUE(stream.good());
}

[[nodiscard]] assets::AudioClipResource MakeClipMetadata(
    const std::filesystem::path& path,
    const assets::AudioClipLoadPolicy policy,
    const std::uint32_t sampleRateHz,
    const std::uint16_t channelCount,
    const std::uint64_t frameCount)
{
    assets::AudioClipResource clip{};
    clip.loadPolicy = policy;
    clip.sampleRateHz = sampleRateHz;
    clip.channelCount = channelCount;
    clip.frameCount = frameCount;
    clip.encodedByteSize = std::filesystem::file_size(path);
    return clip;
}
} // namespace

TEST(AudioClipPreparation2D, PreloadDecodesOnceAndAccountsOwnedPcm)
{
    TemporaryAudioRoot temporary{};
    const std::filesystem::path source = temporary.Path() / "audio" / "short.wav";
    WritePcm16Wav(
        source,
        48000U,
        2U,
        {0, 32767, -32768, 16384, 8192, -8192, 4096, -4096});

    assets::ResourceRegistry resources{temporary.Path()};
    const auto published = resources.PublishAudioClip(
        "audio/short.wav",
        MakeClipMetadata(source, assets::AudioClipLoadPolicy::Preload, 48000U, 2U, 4U));
    ASSERT_TRUE(published.Succeeded());

    AudioClipPreparation2D preparation{resources};
    const AudioClipPrepareResult2D result = preparation.PreparePreloaded(published.handle);
    ASSERT_TRUE(result.Succeeded()) << result.diagnostic;
    EXPECT_EQ(result.prepared.clip, published.handle);
    EXPECT_EQ(result.prepared.sampleRateHz, 48000U);
    EXPECT_EQ(result.prepared.channelCount, 2U);
    EXPECT_EQ(result.prepared.frameCount, 4U);
    ASSERT_EQ(result.prepared.interleavedPcmF32.size(), 8U);
    EXPECT_NEAR(result.prepared.interleavedPcmF32[0], 0.0F, 0.0001F);
    EXPECT_GT(result.prepared.interleavedPcmF32[1], 0.99F);
    EXPECT_LT(result.prepared.interleavedPcmF32[2], -0.99F);

    EXPECT_EQ(result.prepared.metrics.decodedFrameCount, 4U);
    EXPECT_EQ(result.prepared.metrics.readCallCount, 1U);
    EXPECT_EQ(result.prepared.metrics.framesRead, 4U);
    EXPECT_EQ(result.prepared.metrics.trace2dOwnedPcmBytes, 8U * sizeof(float));
    EXPECT_GE(
        result.prepared.metrics.trace2dOwnedPcmCapacityBytes,
        result.prepared.metrics.trace2dOwnedPcmBytes);
    EXPECT_EQ(result.prepared.metrics.decoderObjectBytes, 0U);
    EXPECT_FALSE(result.prepared.metrics.decoderInternalBytesKnown);
    EXPECT_FALSE(result.prepared.metrics.sourceIoMayBlock);
}

TEST(AudioClipPreparation2D, StreamReadsCallerBoundedFramesAndSeeksWithoutRetainedPcm)
{
    TemporaryAudioRoot temporary{};
    const std::filesystem::path source = temporary.Path() / "audio" / "music.wav";
    WritePcm16Wav(
        source,
        24000U,
        2U,
        {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000,
         8000, 9000, 10000, 11000, 12000, 13000, 14000, 15000});

    assets::ResourceRegistry resources{temporary.Path()};
    const auto published = resources.PublishAudioClip(
        "audio/music.wav",
        MakeClipMetadata(source, assets::AudioClipLoadPolicy::Stream, 24000U, 2U, 8U));
    ASSERT_TRUE(published.Succeeded());

    AudioClipPreparation2D preparation{resources};
    AudioClipStreamOpenResult2D opened = preparation.OpenStream(published.handle);
    ASSERT_TRUE(opened.Succeeded()) << opened.diagnostic;
    ASSERT_TRUE(opened.stream->IsOpen());
    EXPECT_EQ(opened.stream->SampleRateHz(), 24000U);
    EXPECT_EQ(opened.stream->ChannelCount(), 2U);
    EXPECT_EQ(opened.stream->FrameCount(), 8U);

    std::array<float, 6> first{};
    std::uint64_t framesRead = 0U;
    EXPECT_EQ(
        opened.stream->ReadFrames(first, 3U, framesRead),
        AudioClipPreparationResult2D::Success);
    EXPECT_EQ(framesRead, 3U);

    std::array<float, 2> tooSmall{};
    EXPECT_EQ(
        opened.stream->ReadFrames(tooSmall, 2U, framesRead),
        AudioClipPreparationResult2D::InvalidBuffer);
    EXPECT_EQ(framesRead, 0U);

    EXPECT_EQ(opened.stream->Seek(1U), AudioClipPreparationResult2D::Success);
    std::array<float, 4> second{};
    EXPECT_EQ(
        opened.stream->ReadFrames(second, 2U, framesRead),
        AudioClipPreparationResult2D::Success);
    EXPECT_EQ(framesRead, 2U);
    EXPECT_EQ(opened.stream->Seek(9U), AudioClipPreparationResult2D::SeekOutOfRange);

    const AudioClipPreparationMetrics2D metrics = opened.stream->Metrics();
    EXPECT_EQ(metrics.readCallCount, 2U);
    EXPECT_EQ(metrics.seekCallCount, 1U);
    EXPECT_EQ(metrics.framesRead, 5U);
    EXPECT_EQ(metrics.trace2dOwnedPcmBytes, 0U);
    EXPECT_EQ(metrics.trace2dOwnedPcmCapacityBytes, 0U);
    EXPECT_GT(metrics.decoderObjectBytes, 0U);
    EXPECT_FALSE(metrics.decoderInternalBytesKnown);
    EXPECT_TRUE(metrics.sourceIoMayBlock);
}

TEST(AudioClipPreparation2D, RejectsPolicyMismatchMetadataMismatchAndStaleResource)
{
    TemporaryAudioRoot temporary{};
    const std::filesystem::path source = temporary.Path() / "audio" / "mismatch.wav";
    WritePcm16Wav(source, 48000U, 1U, {0, 1000, 2000, 3000});

    assets::ResourceRegistry resources{temporary.Path()};
    const auto streamClip = resources.PublishAudioClip(
        "audio/mismatch.wav",
        MakeClipMetadata(source, assets::AudioClipLoadPolicy::Stream, 48000U, 1U, 4U));
    ASSERT_TRUE(streamClip.Succeeded());

    AudioClipPreparation2D preparation{resources};
    const AudioClipPrepareResult2D wrongPolicy = preparation.PreparePreloaded(streamClip.handle);
    EXPECT_EQ(wrongPolicy.result, AudioClipPreparationResult2D::PolicyMismatch);

    ASSERT_TRUE(resources.Unload(streamClip.handle.Untyped()).Succeeded());
    const AudioClipStreamOpenResult2D stale = preparation.OpenStream(streamClip.handle);
    EXPECT_EQ(stale.result, AudioClipPreparationResult2D::ResourceUnavailable);

    const auto mismatch = resources.PublishAudioClip(
        "audio/mismatch.wav",
        MakeClipMetadata(source, assets::AudioClipLoadPolicy::Preload, 44100U, 1U, 4U));
    ASSERT_TRUE(mismatch.Succeeded());
    const AudioClipPrepareResult2D badMetadata = preparation.PreparePreloaded(mismatch.handle);
    EXPECT_EQ(badMetadata.result, AudioClipPreparationResult2D::MetadataMismatch);
    EXPECT_TRUE(badMetadata.prepared.interleavedPcmF32.empty());
}
} // namespace trace2d::audio
