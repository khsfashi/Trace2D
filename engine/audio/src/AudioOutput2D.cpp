#include "AudioOutput2DInternal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace trace2d::audio
{
namespace
{
constexpr std::size_t MaximumOutputVoiceCapacity2D = 4096U;
constexpr std::size_t MaximumPreloadCacheCapacity2D = 256U;
constexpr std::size_t MaximumAudioBufferByteBudget2D = 1024U * 1024U * 1024U;
constexpr std::uint32_t MaximumRefillChunkFrames2D = 262144U;
constexpr std::uint32_t MaximumTargetQueuedFrames2D = 1048576U;
constexpr std::uint32_t MaximumRefillChunksPerPump2D = 64U;
}

bool AudioOutputCheckedSampleCount(
    const std::uint64_t frames,
    const std::uint16_t channels,
    std::size_t& samples) noexcept
{
    if (channels == 0U || frames > std::numeric_limits<std::size_t>::max() / channels)
    {
        return false;
    }
    samples = static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
    return true;
}

bool AudioOutputCheckedByteCount(
    const std::uint64_t frames,
    const std::uint16_t channels,
    std::size_t& bytes) noexcept
{
    std::size_t samples = 0U;
    if (!AudioOutputCheckedSampleCount(frames, channels, samples) ||
        samples > std::numeric_limits<std::size_t>::max() / sizeof(float))
    {
        return false;
    }
    bytes = samples * sizeof(float);
    return true;
}

std::uint64_t AudioOutputClampFrameIndex(
    const double positionFrames,
    const std::uint64_t frameCount) noexcept
{
    if (frameCount == 0U || !std::isfinite(positionFrames) || positionFrames <= 0.0)
    {
        return 0U;
    }
    const double maximum = static_cast<double>(frameCount - 1U);
    return static_cast<std::uint64_t>(std::floor(std::min(positionFrames, maximum)));
}

std::string_view ToString(const AudioOutputState2D value) noexcept
{
    switch (value)
    {
    case AudioOutputState2D::Stopped: return "stopped";
    case AudioOutputState2D::Running: return "running";
    case AudioOutputState2D::Suspended: return "suspended";
    case AudioOutputState2D::RecoveryPending: return "recovery_pending";
    }
    return "unknown";
}

std::string_view ToString(const AudioOutputResult2D value) noexcept
{
    switch (value)
    {
    case AudioOutputResult2D::Success: return "success";
    case AudioOutputResult2D::AlreadyStarted: return "already_started";
    case AudioOutputResult2D::NotStarted: return "not_started";
    case AudioOutputResult2D::InvalidConfig: return "invalid_config";
    case AudioOutputResult2D::BackendUnavailable: return "backend_unavailable";
    case AudioOutputResult2D::DeviceOpenFailed: return "device_open_failed";
    case AudioOutputResult2D::CapacityExceeded: return "capacity_exceeded";
    case AudioOutputResult2D::ResourceUnavailable: return "resource_unavailable";
    case AudioOutputResult2D::ClipPreparationFailed: return "clip_preparation_failed";
    case AudioOutputResult2D::StreamOpenFailed: return "stream_open_failed";
    case AudioOutputResult2D::StreamSeekFailed: return "stream_seek_failed";
    case AudioOutputResult2D::StreamCreateFailed: return "stream_create_failed";
    case AudioOutputResult2D::StreamBindFailed: return "stream_bind_failed";
    case AudioOutputResult2D::StreamUpdateFailed: return "stream_update_failed";
    case AudioOutputResult2D::DeviceControlFailed: return "device_control_failed";
    case AudioOutputResult2D::RecoveryFailed: return "recovery_failed";
    }
    return "unknown";
}

bool SDLCALL AudioOutput2D::Impl::DeviceEventWatch(void* userdata, SDL_Event* event) noexcept
{
    if (userdata == nullptr || event == nullptr)
    {
        return true;
    }

    if (event->type != SDL_EVENT_AUDIO_DEVICE_REMOVED &&
        event->type != SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED)
    {
        return true;
    }
    if (event->adevice.recording)
    {
        return true;
    }

    auto& output = *static_cast<Impl*>(userdata);
    const SDL_AudioDeviceID watchedDevice = output.watchedDevice_.load(std::memory_order_relaxed);
    if (watchedDevice == 0U || event->adevice.which != watchedDevice)
    {
        // SDL3 migrates logical devices opened as the system default between physical devices.
        // Physical-device and unrelated logical-device events therefore must not force Trace2D
        // to tear down and rebuild its presentation streams.
        return true;
    }

    if (event->type == SDL_EVENT_AUDIO_DEVICE_REMOVED)
    {
        output.pendingDeviceLossEventCount_.fetch_add(1U, std::memory_order_relaxed);
    }
    else
    {
        output.pendingDeviceFormatChangeEventCount_.fetch_add(1U, std::memory_order_relaxed);
    }
    return true;
}

AudioOutput2D::Impl::Impl(const assets::ResourceRegistry& resources, AudioOutputConfig2D config)
    : resources_{resources}
    , preparation_{resources}
    , config_{config}
{
    if (config_.voiceCapacity > 0U && config_.voiceCapacity <= MaximumOutputVoiceCapacity2D)
    {
        voices_.reserve(config_.voiceCapacity);
    }
    if (config_.preloadCacheCapacity > 0U && config_.preloadCacheCapacity <= MaximumPreloadCacheCapacity2D)
    {
        preloadCache_.reserve(config_.preloadCacheCapacity);
    }
}

AudioOutput2D::Impl::~Impl()
{
    Stop();
    for (PhysicalVoice& voice : voices_)
    {
        ReleaseVoicePreparation(voice);
    }
}

bool AudioOutput2D::Impl::ValidateConfig() const noexcept
{
    if (config_.voiceCapacity == 0U || config_.voiceCapacity > MaximumOutputVoiceCapacity2D ||
        config_.preloadCacheCapacity == 0U || config_.preloadCacheCapacity > MaximumPreloadCacheCapacity2D ||
        config_.preloadPcmByteBudget == 0U || config_.preloadPcmByteBudget > MaximumAudioBufferByteBudget2D ||
        config_.refillBufferByteBudget == 0U || config_.refillBufferByteBudget > MaximumAudioBufferByteBudget2D ||
        config_.refillChunkFrames == 0U || config_.refillChunkFrames > MaximumRefillChunkFrames2D ||
        config_.targetQueuedFrames == 0U || config_.targetQueuedFrames > MaximumTargetQueuedFrames2D ||
        config_.maxRefillChunksPerPump == 0U || config_.maxRefillChunksPerPump > MaximumRefillChunksPerPump2D)
    {
        return false;
    }

    std::size_t maximumChunkBytes = 0U;
    return AudioOutputCheckedByteCount(
               config_.refillChunkFrames,
               assets::MaximumAudioClipChannelCount,
               maximumChunkBytes) &&
        maximumChunkBytes <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
        maximumChunkBytes <= config_.refillBufferByteBudget;
}

AudioOutputResult2D AudioOutput2D::Impl::Start()
{
    if (state_ != AudioOutputState2D::Stopped)
    {
        return AudioOutputResult2D::AlreadyStarted;
    }
    if (!ValidateConfig())
    {
        diagnostic_ = "AudioOutput2D config exceeds bounded voice/cache/refill limits.";
        return AudioOutputResult2D::InvalidConfig;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL audio initialization failed: "} + SDL_GetError();
        return AudioOutputResult2D::BackendUnavailable;
    }
    audioSubsystemInitialized_ = true;
    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0U)
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL default playback device open failed: "} + SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystemInitialized_ = false;
        return AudioOutputResult2D::DeviceOpenFailed;
    }

    watchedDevice_.store(device_, std::memory_order_relaxed);
    if (!SDL_AddEventWatch(&AudioOutput2D::Impl::DeviceEventWatch, this))
    {
        watchedDevice_.store(0U, std::memory_order_relaxed);
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL audio-device event watch registration failed: "} + SDL_GetError();
        SDL_CloseAudioDevice(device_);
        device_ = 0U;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystemInitialized_ = false;
        return AudioOutputResult2D::BackendUnavailable;
    }
    eventWatchRegistered_ = true;
    pendingDeviceLossEventCount_.store(0U, std::memory_order_relaxed);
    pendingDeviceFormatChangeEventCount_.store(0U, std::memory_order_relaxed);

    ++deviceOpenCount_;
    state_ = AudioOutputState2D::Running;
    resumeSuspendedAfterRecovery_ = false;
    diagnostic_.clear();
    return AudioOutputResult2D::Success;
}

void AudioOutput2D::Impl::Stop() noexcept
{
    watchedDevice_.store(0U, std::memory_order_relaxed);
    if (eventWatchRegistered_)
    {
        SDL_RemoveEventWatch(&AudioOutput2D::Impl::DeviceEventWatch, this);
        eventWatchRegistered_ = false;
    }
    pendingDeviceLossEventCount_.store(0U, std::memory_order_relaxed);
    pendingDeviceFormatChangeEventCount_.store(0U, std::memory_order_relaxed);

    for (PhysicalVoice& voice : voices_)
    {
        DestroyStream(voice);
    }
    if (device_ != 0U)
    {
        SDL_CloseAudioDevice(device_);
        device_ = 0U;
    }
    if (audioSubsystemInitialized_)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioSubsystemInitialized_ = false;
    }
    state_ = AudioOutputState2D::Stopped;
    resumeSuspendedAfterRecovery_ = false;
}

AudioOutputResult2D AudioOutput2D::Impl::Suspend()
{
    if (state_ == AudioOutputState2D::Stopped || device_ == 0U)
    {
        return AudioOutputResult2D::NotStarted;
    }
    if (state_ == AudioOutputState2D::RecoveryPending)
    {
        resumeSuspendedAfterRecovery_ = true;
        return AudioOutputResult2D::Success;
    }
    if (state_ == AudioOutputState2D::Suspended)
    {
        return AudioOutputResult2D::Success;
    }
    if (!SDL_PauseAudioDevice(device_))
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL playback suspend failed: "} + SDL_GetError();
        return AudioOutputResult2D::DeviceControlFailed;
    }
    ++deviceSuspendCount_;
    state_ = AudioOutputState2D::Suspended;
    return AudioOutputResult2D::Success;
}

AudioOutputResult2D AudioOutput2D::Impl::Resume()
{
    if (state_ == AudioOutputState2D::Stopped || device_ == 0U)
    {
        return AudioOutputResult2D::NotStarted;
    }
    if (state_ == AudioOutputState2D::RecoveryPending)
    {
        resumeSuspendedAfterRecovery_ = false;
        return AudioOutputResult2D::Success;
    }
    if (state_ == AudioOutputState2D::Running)
    {
        return AudioOutputResult2D::Success;
    }
    if (!SDL_ResumeAudioDevice(device_))
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL playback resume failed: "} + SDL_GetError();
        return AudioOutputResult2D::DeviceControlFailed;
    }
    ++deviceResumeCount_;
    state_ = AudioOutputState2D::Running;
    return AudioOutputResult2D::Success;
}

AudioOutputDeviceEventReport2D AudioOutput2D::Impl::PollDeviceEvents()
{
    AudioOutputDeviceEventReport2D report{};
    if (state_ == AudioOutputState2D::Stopped)
    {
        report.result = AudioOutputResult2D::NotStarted;
        return report;
    }

    const std::uint64_t deviceLossCount =
        pendingDeviceLossEventCount_.exchange(0U, std::memory_order_relaxed);
    const std::uint64_t formatChangeCount =
        pendingDeviceFormatChangeEventCount_.exchange(0U, std::memory_order_relaxed);

    const auto boundedSize = [](const std::uint64_t value) noexcept -> std::size_t {
        if constexpr (sizeof(std::size_t) >= sizeof(std::uint64_t))
        {
            return static_cast<std::size_t>(value);
        }
        else
        {
            const std::uint64_t maximum = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
            return value > maximum ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(value);
        }
    };

    const std::size_t boundedLossCount = boundedSize(deviceLossCount);
    const std::size_t boundedFormatChangeCount = boundedSize(formatChangeCount);
    const std::size_t maximumProcessedCount = std::numeric_limits<std::size_t>::max();
    report.processedEventCount =
        boundedLossCount > maximumProcessedCount - boundedFormatChangeCount
        ? maximumProcessedCount
        : boundedLossCount + boundedFormatChangeCount;

    deviceLossEventCount_ += deviceLossCount;
    deviceFormatChangeEventCount_ += formatChangeCount;
    report.recoveryRequested = deviceLossCount != 0U || formatChangeCount != 0U;

    if (report.recoveryRequested)
    {
        resumeSuspendedAfterRecovery_ = state_ == AudioOutputState2D::Suspended;
        state_ = AudioOutputState2D::RecoveryPending;
    }
    return report;
}

AudioOutputResult2D AudioOutput2D::Impl::Recover(const AudioSystem2D& semantic)
{
    if (state_ == AudioOutputState2D::Stopped || !audioSubsystemInitialized_)
    {
        return AudioOutputResult2D::NotStarted;
    }

    // SDL_RemoveEventWatch serializes with dispatch of this watch. Unregistering before replacing
    // the logical device guarantees that an old-device callback cannot publish a stale recovery
    // signal after the pending counters are cleared.
    watchedDevice_.store(0U, std::memory_order_relaxed);
    if (eventWatchRegistered_)
    {
        SDL_RemoveEventWatch(&AudioOutput2D::Impl::DeviceEventWatch, this);
        eventWatchRegistered_ = false;
    }
    pendingDeviceLossEventCount_.store(0U, std::memory_order_relaxed);
    pendingDeviceFormatChangeEventCount_.store(0U, std::memory_order_relaxed);

    for (PhysicalVoice& voice : voices_)
    {
        DestroyStream(voice);
    }
    if (device_ != 0U)
    {
        SDL_CloseAudioDevice(device_);
        device_ = 0U;
    }

    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (device_ == 0U)
    {
        ++backendFailureCount_;
        state_ = AudioOutputState2D::RecoveryPending;
        diagnostic_ = std::string{"SDL playback device recovery failed: "} + SDL_GetError();
        return AudioOutputResult2D::RecoveryFailed;
    }

    watchedDevice_.store(device_, std::memory_order_relaxed);
    if (!SDL_AddEventWatch(&AudioOutput2D::Impl::DeviceEventWatch, this))
    {
        watchedDevice_.store(0U, std::memory_order_relaxed);
        ++backendFailureCount_;
        SDL_CloseAudioDevice(device_);
        device_ = 0U;
        state_ = AudioOutputState2D::RecoveryPending;
        diagnostic_ = std::string{"SDL audio-device event watch recovery failed: "} + SDL_GetError();
        return AudioOutputResult2D::RecoveryFailed;
    }
    eventWatchRegistered_ = true;
    ++deviceOpenCount_;

    std::size_t index = 0U;
    while (index < voices_.size())
    {
        const std::optional<AudioVoiceSnapshot2D> snapshot = semantic.InspectVoice(voices_[index].handle);
        if (!snapshot.has_value())
        {
            RemoveVoice(index);
            continue;
        }
        PhysicalVoice& voice = voices_[index];
        UpdateFromSnapshot(voice, *snapshot);
        voice.nextFrame = AudioOutputClampFrameIndex(voice.semanticPositionFrames, voice.frameCount);
        voice.reachedEnd = false;
        if (voice.decoder != nullptr &&
            voice.decoder->Seek(voice.nextFrame) != AudioClipPreparationResult2D::Success)
        {
            ++backendFailureCount_;
            state_ = AudioOutputState2D::RecoveryPending;
            diagnostic_ = "Streaming decoder seek failed while rebuilding physical audio.";
            return AudioOutputResult2D::StreamSeekFailed;
        }
        ++index;
    }

    if (resumeSuspendedAfterRecovery_ && !SDL_PauseAudioDevice(device_))
    {
        ++backendFailureCount_;
        state_ = AudioOutputState2D::RecoveryPending;
        diagnostic_ = std::string{"Recovered SDL device could not be suspended: "} + SDL_GetError();
        return AudioOutputResult2D::RecoveryFailed;
    }
    state_ = resumeSuspendedAfterRecovery_ ? AudioOutputState2D::Suspended : AudioOutputState2D::Running;
    resumeSuspendedAfterRecovery_ = false;
    ++recoveryCount_;
    diagnostic_.clear();
    return AudioOutputResult2D::Success;
}

AudioOutputState2D AudioOutput2D::Impl::State() const noexcept { return state_; }
const AudioOutputConfig2D& AudioOutput2D::Impl::Config() const noexcept { return config_; }
std::string_view AudioOutput2D::Impl::LastDiagnostic() const noexcept { return diagnostic_; }

AudioOutput2D::AudioOutput2D(
    const assets::ResourceRegistry& resources,
    const AudioOutputConfig2D config)
    : impl_{std::make_unique<Impl>(resources, config)}
{
}
AudioOutput2D::~AudioOutput2D() = default;
AudioOutputResult2D AudioOutput2D::Start() { return impl_->Start(); }
void AudioOutput2D::Stop() noexcept { impl_->Stop(); }
AudioOutputResult2D AudioOutput2D::Suspend() { return impl_->Suspend(); }
AudioOutputResult2D AudioOutput2D::Resume() { return impl_->Resume(); }
AudioOutputSyncReport2D AudioOutput2D::Sync(
    const AudioSystem2D& semantic,
    const std::span<const AudioEvent2D> events)
{
    return impl_->Sync(semantic, events);
}
AudioOutputPumpReport2D AudioOutput2D::Pump() { return impl_->Pump(); }
AudioOutputDeviceEventReport2D AudioOutput2D::PollDeviceEvents() { return impl_->PollDeviceEvents(); }
AudioOutputResult2D AudioOutput2D::Recover(const AudioSystem2D& semantic) { return impl_->Recover(semantic); }
AudioOutputState2D AudioOutput2D::State() const noexcept { return impl_->State(); }
const AudioOutputConfig2D& AudioOutput2D::Config() const noexcept { return impl_->Config(); }
AudioOutputMetrics2D AudioOutput2D::Metrics() const noexcept { return impl_->Metrics(); }
std::string_view AudioOutput2D::LastDiagnostic() const noexcept { return impl_->LastDiagnostic(); }
} // namespace trace2d::audio
