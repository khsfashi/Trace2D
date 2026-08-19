#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::audio
{
enum class AudioClipPreparationResult2D : std::uint8_t
{
    Success = 0,
    ResourceUnavailable,
    PolicyMismatch,
    SourceOpenFailed,
    MetadataMismatch,
    DecodeFailed,
    InvalidBuffer,
    SeekOutOfRange,
    SeekFailed,
};

[[nodiscard]] std::string_view ToString(AudioClipPreparationResult2D value) noexcept;

// Exact Trace2D-owned memory is reported separately from opaque decoder internals. The latter are
// intentionally not guessed: dependency-internal allocations remain explicitly unknown until a
// future backend can measure them reliably.
struct AudioClipPreparationMetrics2D final
{
    std::uint64_t decodedFrameCount{0U};
    std::uint64_t readCallCount{0U};
    std::uint64_t seekCallCount{0U};
    std::uint64_t framesRead{0U};
    std::size_t trace2dOwnedPcmBytes{0U};
    std::size_t trace2dOwnedPcmCapacityBytes{0U};
    std::size_t decoderObjectBytes{0U};
    bool decoderInternalBytesKnown{false};
    bool sourceIoMayBlock{false};
};

struct PreparedAudioClip2D final
{
    assets::ResourceHandle<assets::AudioClipResource> clip{};
    std::uint32_t sampleRateHz{0U};
    std::uint16_t channelCount{0U};
    std::uint64_t frameCount{0U};
    std::vector<float> interleavedPcmF32{};
    AudioClipPreparationMetrics2D metrics{};
};

struct AudioClipPrepareResult2D final
{
    AudioClipPreparationResult2D result{AudioClipPreparationResult2D::Success};
    PreparedAudioClip2D prepared{};
    std::string diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return result == AudioClipPreparationResult2D::Success;
    }
};

class StreamingAudioClip2D final
{
public:
    StreamingAudioClip2D(const StreamingAudioClip2D&) = delete;
    StreamingAudioClip2D& operator=(const StreamingAudioClip2D&) = delete;
    StreamingAudioClip2D(StreamingAudioClip2D&&) noexcept;
    StreamingAudioClip2D& operator=(StreamingAudioClip2D&&) noexcept;
    ~StreamingAudioClip2D();

    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] assets::ResourceHandle<assets::AudioClipResource> Clip() const noexcept;
    [[nodiscard]] std::uint32_t SampleRateHz() const noexcept;
    [[nodiscard]] std::uint16_t ChannelCount() const noexcept;
    [[nodiscard]] std::uint64_t FrameCount() const noexcept;

    // This is an explicit decode/I/O operation. It can block on source-file I/O and must therefore
    // run on setup/stream-worker code, never inside AudioSystem2D::Step() or a gameplay hot path.
    [[nodiscard]] AudioClipPreparationResult2D ReadFrames(
        std::span<float> interleavedOutput,
        std::uint64_t requestedFrames,
        std::uint64_t& framesRead) noexcept;

    [[nodiscard]] AudioClipPreparationResult2D Seek(std::uint64_t frameIndex) noexcept;
    [[nodiscard]] AudioClipPreparationMetrics2D Metrics() const noexcept;

private:
    struct Impl;

    StreamingAudioClip2D(
        assets::ResourceHandle<assets::AudioClipResource> clip,
        std::uint32_t sampleRateHz,
        std::uint16_t channelCount,
        std::uint64_t frameCount,
        std::unique_ptr<Impl> impl) noexcept;

    assets::ResourceHandle<assets::AudioClipResource> clip_{};
    std::uint32_t sampleRateHz_{0U};
    std::uint16_t channelCount_{0U};
    std::uint64_t frameCount_{0U};
    std::unique_ptr<Impl> impl_{};
    AudioClipPreparationMetrics2D metrics_{};

    friend class AudioClipPreparation2D;
};

struct AudioClipStreamOpenResult2D final
{
    AudioClipPreparationResult2D result{AudioClipPreparationResult2D::Success};
    std::unique_ptr<StreamingAudioClip2D> stream{};
    std::string diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return result == AudioClipPreparationResult2D::Success && stream != nullptr;
    }
};

class AudioClipPreparation2D final
{
public:
    explicit AudioClipPreparation2D(const assets::ResourceRegistry& resources) noexcept;

    // Preload decodes once during explicit preparation and returns retained interleaved float PCM.
    [[nodiscard]] AudioClipPrepareResult2D PreparePreloaded(
        assets::ResourceHandle<assets::AudioClipResource> clip) const;

    // Streaming keeps only decoder state. PCM storage is caller-owned and bounded per ReadFrames().
    [[nodiscard]] AudioClipStreamOpenResult2D OpenStream(
        assets::ResourceHandle<assets::AudioClipResource> clip) const;

private:
    const assets::ResourceRegistry& resources_;
};
} // namespace trace2d::audio
