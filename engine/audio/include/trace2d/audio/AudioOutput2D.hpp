#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace trace2d::audio
{
enum class AudioOutputState2D : std::uint8_t
{
    Stopped = 0,
    Running,
    Suspended,
    RecoveryPending,
};

[[nodiscard]] std::string_view ToString(AudioOutputState2D value) noexcept;

enum class AudioOutputResult2D : std::uint8_t
{
    Success = 0,
    AlreadyStarted,
    NotStarted,
    InvalidConfig,
    BackendUnavailable,
    DeviceOpenFailed,
    CapacityExceeded,
    ResourceUnavailable,
    ClipPreparationFailed,
    StreamOpenFailed,
    StreamSeekFailed,
    StreamCreateFailed,
    StreamBindFailed,
    StreamUpdateFailed,
    DeviceControlFailed,
    RecoveryFailed,
};

[[nodiscard]] std::string_view ToString(AudioOutputResult2D value) noexcept;

struct AudioOutputConfig2D final
{
    std::size_t voiceCapacity{64U};
    std::size_t preloadCacheCapacity{32U};
    std::size_t preloadPcmByteBudget{32U * 1024U * 1024U};
    std::size_t refillBufferByteBudget{8U * 1024U * 1024U};
    std::uint32_t refillChunkFrames{2048U};
    std::uint32_t targetQueuedFrames{4096U};
    std::uint32_t maxRefillChunksPerPump{2U};
};

struct AudioOutputSyncReport2D final
{
    AudioOutputResult2D result{AudioOutputResult2D::Success};
    std::size_t createdVoiceCount{0U};
    std::size_t removedVoiceCount{0U};
    std::size_t updatedVoiceCount{0U};
};

struct AudioOutputPumpReport2D final
{
    AudioOutputResult2D result{AudioOutputResult2D::Success};
    std::size_t visitedVoiceCount{0U};
    std::size_t preparedVoiceCount{0U};
    std::size_t refillChunkCount{0U};
    std::uint64_t refillFrameCount{0U};
};

struct AudioOutputDeviceEventReport2D final
{
    AudioOutputResult2D result{AudioOutputResult2D::Success};
    std::size_t processedEventCount{0U};
    bool recoveryRequested{false};
};

struct AudioOutputMetrics2D final
{
    AudioOutputState2D state{AudioOutputState2D::Stopped};
    std::size_t configuredVoiceCapacity{0U};
    std::size_t trackedVoiceCount{0U};
    std::size_t activeStreamCount{0U};
    std::size_t streamingVoiceCount{0U};
    std::size_t preloadCacheEntryCount{0U};
    std::size_t preloadCacheCapacity{0U};
    std::size_t preloadPcmByteBudget{0U};
    std::size_t refillBufferByteBudget{0U};
    std::size_t trace2dOwnedPreloadPcmBytes{0U};
    std::size_t trace2dOwnedPreloadPcmCapacityBytes{0U};
    std::size_t trace2dOwnedRefillBytes{0U};
    std::size_t trace2dOwnedRefillCapacityBytes{0U};
    std::uint64_t queuedInputBytes{0U};
    std::uint64_t deviceOpenCount{0U};
    std::uint64_t deviceSuspendCount{0U};
    std::uint64_t deviceResumeCount{0U};
    std::uint64_t deviceLossEventCount{0U};
    std::uint64_t deviceFormatChangeEventCount{0U};
    std::uint64_t recoveryCount{0U};
    std::uint64_t streamCreateCount{0U};
    std::uint64_t streamDestroyCount{0U};
    std::uint64_t refillCallCount{0U};
    std::uint64_t refillFrameCount{0U};
    std::uint64_t backendFailureCount{0U};
};

// Physical audio is presentation-only. AudioSystem2D remains semantic authority.
// Sync() consumes the semantic event stream and cheap voice snapshots; it never decodes files.
// Pump() may perform file/decode work for first preparation and streaming refill, so callers must
// run Pump() outside AudioSystem2D::Step() and gameplay hot paths.
class AudioOutput2D final
{
public:
    explicit AudioOutput2D(
        const assets::ResourceRegistry& resources,
        AudioOutputConfig2D config = {});
    ~AudioOutput2D();

    AudioOutput2D(const AudioOutput2D&) = delete;
    AudioOutput2D& operator=(const AudioOutput2D&) = delete;
    AudioOutput2D(AudioOutput2D&&) = delete;
    AudioOutput2D& operator=(AudioOutput2D&&) = delete;

    [[nodiscard]] AudioOutputResult2D Start();
    void Stop() noexcept;
    [[nodiscard]] AudioOutputResult2D Suspend();
    [[nodiscard]] AudioOutputResult2D Resume();

    [[nodiscard]] AudioOutputSyncReport2D Sync(
        const AudioSystem2D& semantic,
        std::span<const AudioEvent2D> events);
    [[nodiscard]] AudioOutputPumpReport2D Pump();

    // Device polling drains counters populated by a non-consuming SDL event watch. It never
    // removes events from SDL's shared queue, so Platform can remain the queue consumer.
    // Recovery is explicit so the caller can keep it off deterministic simulation paths.
    [[nodiscard]] AudioOutputDeviceEventReport2D PollDeviceEvents();
    [[nodiscard]] AudioOutputResult2D Recover(const AudioSystem2D& semantic);

    [[nodiscard]] AudioOutputState2D State() const noexcept;
    [[nodiscard]] const AudioOutputConfig2D& Config() const noexcept;
    [[nodiscard]] AudioOutputMetrics2D Metrics() const noexcept;
    [[nodiscard]] std::string_view LastDiagnostic() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace trace2d::audio
