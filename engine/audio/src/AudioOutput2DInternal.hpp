#pragma once

#include <trace2d/audio/AudioClipPreparation2D.hpp>
#include <trace2d/audio/AudioOutput2D.hpp>

#include <SDL3/SDL.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace trace2d::audio
{
class AudioOutput2D::Impl final
{
public:
    Impl(const assets::ResourceRegistry& resources, AudioOutputConfig2D config);
    ~Impl();

    [[nodiscard]] AudioOutputResult2D Start();
    void Stop() noexcept;
    [[nodiscard]] AudioOutputResult2D Suspend();
    [[nodiscard]] AudioOutputResult2D Resume();
    [[nodiscard]] AudioOutputSyncReport2D Sync(
        const AudioSystem2D& semantic,
        std::span<const AudioEvent2D> events);
    [[nodiscard]] AudioOutputPumpReport2D Pump();
    [[nodiscard]] AudioOutputDeviceEventReport2D PollDeviceEvents();
    [[nodiscard]] AudioOutputResult2D Recover(const AudioSystem2D& semantic);
    [[nodiscard]] AudioOutputState2D State() const noexcept;
    [[nodiscard]] const AudioOutputConfig2D& Config() const noexcept;
    [[nodiscard]] AudioOutputMetrics2D Metrics() const noexcept;
    [[nodiscard]] std::string_view LastDiagnostic() const noexcept;

private:
    struct PreloadCacheEntry final
    {
        assets::ResourceHandle<assets::AudioClipResource> clip{};
        PreparedAudioClip2D prepared{};
        std::size_t referenceCount{0U};
        std::uint64_t lastUseOrder{0U};
    };

    struct PhysicalVoice final
    {
        AudioVoiceHandle2D handle{};
        assets::ResourceHandle<assets::AudioClipResource> clip{};
        AudioPlaybackState2D semanticState{AudioPlaybackState2D::Playing};
        float effectiveVolume{1.0F};
        float pitch{1.0F};
        bool loop{false};
        assets::AudioClipLoadPolicy loadPolicy{assets::AudioClipLoadPolicy::Preload};
        double semanticPositionFrames{0.0};
        std::uint32_t sampleRateHz{0U};
        std::uint16_t channelCount{0U};
        std::uint64_t frameCount{0U};
        std::uint64_t nextFrame{0U};
        bool reachedEnd{false};
        bool preloadAcquired{false};
        std::unique_ptr<StreamingAudioClip2D> decoder{};
        std::unique_ptr<float[]> refillBuffer{};
        std::size_t refillSampleCount{0U};
        SDL_AudioStream* stream{nullptr};
        bool bound{false};
    };

    static bool SDLCALL DeviceEventWatch(void* userdata, SDL_Event* event) noexcept;

    [[nodiscard]] bool ValidateConfig() const noexcept;
    [[nodiscard]] std::optional<std::size_t> FindVoice(AudioVoiceHandle2D handle) const noexcept;
    [[nodiscard]] PreloadCacheEntry* FindPreload(
        assets::ResourceHandle<assets::AudioClipResource> clip) noexcept;
    [[nodiscard]] const PreloadCacheEntry* FindPreload(
        assets::ResourceHandle<assets::AudioClipResource> clip) const noexcept;
    [[nodiscard]] std::size_t CurrentPreloadPcmCapacityBytes() const noexcept;
    [[nodiscard]] std::size_t CurrentRefillBytes() const noexcept;
    [[nodiscard]] bool EvictOneUnusedPreload() noexcept;
    [[nodiscard]] AudioOutputResult2D AcquirePreload(PhysicalVoice& voice);
    void ReleaseVoicePreparation(PhysicalVoice& voice) noexcept;
    void DestroyStream(PhysicalVoice& voice) noexcept;
    void RemoveVoice(std::size_t index) noexcept;
    static void UpdateFromSnapshot(PhysicalVoice& voice, const AudioVoiceSnapshot2D& snapshot) noexcept;
    [[nodiscard]] bool IsPrepared(const PhysicalVoice& voice) const noexcept;
    [[nodiscard]] AudioOutputResult2D EnsurePrepared(PhysicalVoice& voice);
    [[nodiscard]] AudioOutputResult2D ApplyStreamState(PhysicalVoice& voice);
    [[nodiscard]] AudioOutputResult2D RefillVoice(
        PhysicalVoice& voice,
        AudioOutputPumpReport2D& report);
    [[nodiscard]] AudioOutputResult2D PutPreloadedChunk(
        PhysicalVoice& voice,
        std::uint64_t& framesWritten);
    [[nodiscard]] AudioOutputResult2D PutStreamingChunk(
        PhysicalVoice& voice,
        std::uint64_t& framesWritten);

    const assets::ResourceRegistry& resources_;
    AudioClipPreparation2D preparation_;
    AudioOutputConfig2D config_{};
    std::vector<PhysicalVoice> voices_{};
    std::vector<PreloadCacheEntry> preloadCache_{};
    SDL_AudioDeviceID device_{0U};
    AudioOutputState2D state_{AudioOutputState2D::Stopped};
    bool audioSubsystemInitialized_{false};
    bool eventWatchRegistered_{false};
    bool resumeSuspendedAfterRecovery_{false};
    std::atomic<std::uint64_t> pendingDeviceLossEventCount_{0U};
    std::atomic<std::uint64_t> pendingDeviceFormatChangeEventCount_{0U};
    std::string diagnostic_{};
    std::uint64_t preloadUseOrder_{0U};
    std::uint64_t deviceOpenCount_{0U};
    std::uint64_t deviceSuspendCount_{0U};
    std::uint64_t deviceResumeCount_{0U};
    std::uint64_t deviceLossEventCount_{0U};
    std::uint64_t deviceFormatChangeEventCount_{0U};
    std::uint64_t recoveryCount_{0U};
    std::uint64_t streamCreateCount_{0U};
    std::uint64_t streamDestroyCount_{0U};
    std::uint64_t refillCallCount_{0U};
    std::uint64_t refillFrameCount_{0U};
    std::uint64_t backendFailureCount_{0U};
};

[[nodiscard]] bool AudioOutputCheckedSampleCount(
    std::uint64_t frames,
    std::uint16_t channels,
    std::size_t& samples) noexcept;
[[nodiscard]] bool AudioOutputCheckedByteCount(
    std::uint64_t frames,
    std::uint16_t channels,
    std::size_t& bytes) noexcept;
[[nodiscard]] std::uint64_t AudioOutputClampFrameIndex(
    double positionFrames,
    std::uint64_t frameCount) noexcept;
} // namespace trace2d::audio
