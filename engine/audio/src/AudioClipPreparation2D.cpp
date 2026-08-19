#include <trace2d/audio/AudioClipPreparation2D.hpp>

#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>
#include <limits>
#include <utility>

namespace trace2d::audio
{
namespace
{
[[nodiscard]] ma_result OpenDecoder(
    const std::filesystem::path& path,
    const ma_decoder_config* config,
    ma_decoder& decoder) noexcept
{
#if defined(_WIN32)
    return ma_decoder_init_file_w(path.c_str(), config, &decoder);
#else
    const std::string nativePath = path.string();
    return ma_decoder_init_file(nativePath.c_str(), config, &decoder);
#endif
}

[[nodiscard]] std::string DecoderError(const std::string_view operation, const ma_result result)
{
    std::string diagnostic{operation};
    diagnostic += ": ";
    diagnostic += ma_result_description(result);
    return diagnostic;
}

struct ResolvedAudioSource final
{
    const assets::AudioClipResource* clip{nullptr};
    std::filesystem::path path{};
};

[[nodiscard]] AudioClipPreparationResult2D ResolveAudioSource(
    const assets::ResourceRegistry& resources,
    const assets::ResourceHandle<assets::AudioClipResource> handle,
    ResolvedAudioSource& source,
    std::string& diagnostic)
{
    source.clip = resources.Resolve(handle);
    if (source.clip == nullptr)
    {
        diagnostic = "audio clip resource is unavailable or stale";
        return AudioClipPreparationResult2D::ResourceUnavailable;
    }

    const auto snapshot = resources.Inspect(handle.Untyped());
    if (!snapshot.has_value() || snapshot->identity.domain != assets::ResourceTypeDomain::AudioClip)
    {
        diagnostic = "audio clip resource identity is unavailable";
        return AudioClipPreparationResult2D::ResourceUnavailable;
    }

    source.path = resources.ProjectRoot() / std::filesystem::path{snapshot->identity.canonicalReference};
    return AudioClipPreparationResult2D::Success;
}

[[nodiscard]] AudioClipPreparationResult2D ValidateSourceMetadata(
    const std::filesystem::path& path,
    const assets::AudioClipResource& expected,
    std::string& diagnostic)
{
    ma_decoder decoder{};
    const ma_result openResult = OpenDecoder(path, nullptr, decoder);
    if (openResult != MA_SUCCESS)
    {
        diagnostic = DecoderError("failed to open audio source", openResult);
        return AudioClipPreparationResult2D::SourceOpenFailed;
    }

    ma_uint64 frameCount = 0U;
    const ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    const ma_uint32 channelCount = decoder.outputChannels;
    const ma_uint32 sampleRateHz = decoder.outputSampleRate;
    ma_decoder_uninit(&decoder);

    if (lengthResult != MA_SUCCESS)
    {
        diagnostic = DecoderError("failed to determine decoded frame count", lengthResult);
        return AudioClipPreparationResult2D::DecodeFailed;
    }

    if (sampleRateHz != expected.sampleRateHz || channelCount != expected.channelCount ||
        frameCount != expected.frameCount)
    {
        diagnostic = "decoded source metadata does not match canonical AudioClipResource";
        return AudioClipPreparationResult2D::MetadataMismatch;
    }

    return AudioClipPreparationResult2D::Success;
}

[[nodiscard]] AudioClipPreparationResult2D OpenF32Decoder(
    const std::filesystem::path& path,
    const assets::AudioClipResource& clip,
    ma_decoder& decoder,
    std::string& diagnostic)
{
    const ma_decoder_config config = ma_decoder_config_init(
        ma_format_f32,
        static_cast<ma_uint32>(clip.channelCount),
        static_cast<ma_uint32>(clip.sampleRateHz));
    const ma_result result = OpenDecoder(path, &config, decoder);
    if (result != MA_SUCCESS)
    {
        diagnostic = DecoderError("failed to open float PCM decoder", result);
        return AudioClipPreparationResult2D::SourceOpenFailed;
    }
    return AudioClipPreparationResult2D::Success;
}

[[nodiscard]] bool SampleCountFits(
    const std::uint64_t frameCount,
    const std::uint16_t channelCount,
    std::size_t& sampleCount) noexcept
{
    if (channelCount == 0U)
    {
        return false;
    }
    const std::uint64_t maximumSamples = static_cast<std::uint64_t>(
        (std::numeric_limits<std::size_t>::max)() / sizeof(float));
    if (frameCount > maximumSamples / channelCount)
    {
        return false;
    }
    sampleCount = static_cast<std::size_t>(frameCount * channelCount);
    return true;
}

[[nodiscard]] std::uint64_t SaturatingAddU64(
    const std::uint64_t left,
    const std::uint64_t right) noexcept
{
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left)
    {
        return (std::numeric_limits<std::uint64_t>::max)();
    }
    return left + right;
}
} // namespace

std::string_view ToString(const AudioClipPreparationResult2D value) noexcept
{
    switch (value)
    {
    case AudioClipPreparationResult2D::Success: return "success";
    case AudioClipPreparationResult2D::ResourceUnavailable: return "resource_unavailable";
    case AudioClipPreparationResult2D::PolicyMismatch: return "policy_mismatch";
    case AudioClipPreparationResult2D::SourceOpenFailed: return "source_open_failed";
    case AudioClipPreparationResult2D::MetadataMismatch: return "metadata_mismatch";
    case AudioClipPreparationResult2D::DecodeFailed: return "decode_failed";
    case AudioClipPreparationResult2D::InvalidBuffer: return "invalid_buffer";
    case AudioClipPreparationResult2D::SeekOutOfRange: return "seek_out_of_range";
    case AudioClipPreparationResult2D::SeekFailed: return "seek_failed";
    }
    return "unknown";
}

struct StreamingAudioClip2D::Impl final
{
    ma_decoder decoder{};
    bool initialized{false};

    ~Impl()
    {
        if (initialized)
        {
            ma_decoder_uninit(&decoder);
        }
    }
};

StreamingAudioClip2D::StreamingAudioClip2D(
    const assets::ResourceHandle<assets::AudioClipResource> clip,
    const std::uint32_t sampleRateHz,
    const std::uint16_t channelCount,
    const std::uint64_t frameCount,
    std::unique_ptr<Impl> impl) noexcept
    : clip_{clip}
    , sampleRateHz_{sampleRateHz}
    , channelCount_{channelCount}
    , frameCount_{frameCount}
    , impl_{std::move(impl)}
{
    metrics_.decoderObjectBytes = sizeof(ma_decoder);
    metrics_.decoderInternalBytesKnown = false;
    metrics_.sourceIoMayBlock = true;
}

StreamingAudioClip2D::StreamingAudioClip2D(StreamingAudioClip2D&&) noexcept = default;
StreamingAudioClip2D& StreamingAudioClip2D::operator=(StreamingAudioClip2D&&) noexcept = default;
StreamingAudioClip2D::~StreamingAudioClip2D() = default;

bool StreamingAudioClip2D::IsOpen() const noexcept
{
    return impl_ != nullptr && impl_->initialized;
}

assets::ResourceHandle<assets::AudioClipResource> StreamingAudioClip2D::Clip() const noexcept
{
    return clip_;
}

std::uint32_t StreamingAudioClip2D::SampleRateHz() const noexcept
{
    return sampleRateHz_;
}

std::uint16_t StreamingAudioClip2D::ChannelCount() const noexcept
{
    return channelCount_;
}

std::uint64_t StreamingAudioClip2D::FrameCount() const noexcept
{
    return frameCount_;
}

AudioClipPreparationResult2D StreamingAudioClip2D::ReadFrames(
    const std::span<float> interleavedOutput,
    const std::uint64_t requestedFrames,
    std::uint64_t& framesRead) noexcept
{
    framesRead = 0U;
    if (!IsOpen())
    {
        return AudioClipPreparationResult2D::ResourceUnavailable;
    }
    if (requestedFrames == 0U)
    {
        return AudioClipPreparationResult2D::Success;
    }

    std::size_t requiredSamples = 0U;
    if (!SampleCountFits(requestedFrames, channelCount_, requiredSamples) ||
        interleavedOutput.size() < requiredSamples)
    {
        return AudioClipPreparationResult2D::InvalidBuffer;
    }

    ma_uint64 decodedFrames = 0U;
    const ma_result result = ma_decoder_read_pcm_frames(
        &impl_->decoder,
        interleavedOutput.data(),
        static_cast<ma_uint64>(requestedFrames),
        &decodedFrames);

    ++metrics_.readCallCount;
    metrics_.framesRead = SaturatingAddU64(metrics_.framesRead, decodedFrames);
    framesRead = decodedFrames;

    if (result != MA_SUCCESS && result != MA_AT_END)
    {
        return AudioClipPreparationResult2D::DecodeFailed;
    }
    return AudioClipPreparationResult2D::Success;
}

AudioClipPreparationResult2D StreamingAudioClip2D::Seek(const std::uint64_t frameIndex) noexcept
{
    if (!IsOpen())
    {
        return AudioClipPreparationResult2D::ResourceUnavailable;
    }
    if (frameIndex > frameCount_)
    {
        return AudioClipPreparationResult2D::SeekOutOfRange;
    }

    ++metrics_.seekCallCount;
    const ma_result result = ma_decoder_seek_to_pcm_frame(&impl_->decoder, static_cast<ma_uint64>(frameIndex));
    return result == MA_SUCCESS
        ? AudioClipPreparationResult2D::Success
        : AudioClipPreparationResult2D::SeekFailed;
}

AudioClipPreparationMetrics2D StreamingAudioClip2D::Metrics() const noexcept
{
    return metrics_;
}

AudioClipPreparation2D::AudioClipPreparation2D(const assets::ResourceRegistry& resources) noexcept
    : resources_{resources}
{
}

AudioClipPrepareResult2D AudioClipPreparation2D::PreparePreloaded(
    const assets::ResourceHandle<assets::AudioClipResource> handle) const
{
    AudioClipPrepareResult2D result{};
    ResolvedAudioSource source{};
    result.result = ResolveAudioSource(resources_, handle, source, result.diagnostic);
    if (result.result != AudioClipPreparationResult2D::Success)
    {
        return result;
    }
    if (source.clip->loadPolicy != assets::AudioClipLoadPolicy::Preload)
    {
        result.result = AudioClipPreparationResult2D::PolicyMismatch;
        result.diagnostic = "PreparePreloaded requires AudioClipLoadPolicy::Preload";
        return result;
    }

    result.result = ValidateSourceMetadata(source.path, *source.clip, result.diagnostic);
    if (result.result != AudioClipPreparationResult2D::Success)
    {
        return result;
    }

    std::size_t sampleCount = 0U;
    if (!SampleCountFits(source.clip->frameCount, source.clip->channelCount, sampleCount))
    {
        result.result = AudioClipPreparationResult2D::DecodeFailed;
        result.diagnostic = "decoded PCM sample count exceeds addressable storage";
        return result;
    }

    ma_decoder decoder{};
    result.result = OpenF32Decoder(source.path, *source.clip, decoder, result.diagnostic);
    if (result.result != AudioClipPreparationResult2D::Success)
    {
        return result;
    }

    result.prepared.clip = handle;
    result.prepared.sampleRateHz = source.clip->sampleRateHz;
    result.prepared.channelCount = source.clip->channelCount;
    result.prepared.frameCount = source.clip->frameCount;
    result.prepared.interleavedPcmF32.resize(sampleCount);

    ma_uint64 framesRead = 0U;
    const ma_result decodeResult = ma_decoder_read_pcm_frames(
        &decoder,
        result.prepared.interleavedPcmF32.data(),
        static_cast<ma_uint64>(source.clip->frameCount),
        &framesRead);
    ma_decoder_uninit(&decoder);

    if ((decodeResult != MA_SUCCESS && decodeResult != MA_AT_END) || framesRead != source.clip->frameCount)
    {
        result.result = AudioClipPreparationResult2D::DecodeFailed;
        result.diagnostic = DecoderError("failed to decode complete preload PCM", decodeResult);
        result.prepared = {};
        return result;
    }

    result.prepared.metrics.decodedFrameCount = framesRead;
    result.prepared.metrics.readCallCount = 1U;
    result.prepared.metrics.framesRead = framesRead;
    result.prepared.metrics.trace2dOwnedPcmBytes =
        result.prepared.interleavedPcmF32.size() * sizeof(float);
    result.prepared.metrics.trace2dOwnedPcmCapacityBytes =
        result.prepared.interleavedPcmF32.capacity() * sizeof(float);
    result.prepared.metrics.decoderObjectBytes = 0U;
    result.prepared.metrics.decoderInternalBytesKnown = false;
    result.prepared.metrics.sourceIoMayBlock = false;
    result.diagnostic.clear();
    return result;
}

AudioClipStreamOpenResult2D AudioClipPreparation2D::OpenStream(
    const assets::ResourceHandle<assets::AudioClipResource> handle) const
{
    AudioClipStreamOpenResult2D result{};
    ResolvedAudioSource source{};
    result.result = ResolveAudioSource(resources_, handle, source, result.diagnostic);
    if (result.result != AudioClipPreparationResult2D::Success)
    {
        return result;
    }
    if (source.clip->loadPolicy != assets::AudioClipLoadPolicy::Stream)
    {
        result.result = AudioClipPreparationResult2D::PolicyMismatch;
        result.diagnostic = "OpenStream requires AudioClipLoadPolicy::Stream";
        return result;
    }

    result.result = ValidateSourceMetadata(source.path, *source.clip, result.diagnostic);
    if (result.result != AudioClipPreparationResult2D::Success)
    {
        return result;
    }

    auto impl = std::make_unique<StreamingAudioClip2D::Impl>();
    result.result = OpenF32Decoder(source.path, *source.clip, impl->decoder, result.diagnostic);
    if (result.result != AudioClipPreparationResult2D::Success)
    {
        return result;
    }
    impl->initialized = true;

    result.stream = std::unique_ptr<StreamingAudioClip2D>{new StreamingAudioClip2D{
        handle,
        source.clip->sampleRateHz,
        source.clip->channelCount,
        source.clip->frameCount,
        std::move(impl)}};
    result.diagnostic.clear();
    return result;
}
} // namespace trace2d::audio
