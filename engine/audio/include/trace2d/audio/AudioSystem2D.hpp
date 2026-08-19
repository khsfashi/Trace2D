#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace trace2d::audio
{
struct AudioVoiceHandle2D final
{
    static constexpr std::uint32_t InvalidSlot = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t slot{InvalidSlot};
    std::uint32_t generation{0U};

    [[nodiscard]] bool IsValid() const noexcept
    {
        return slot != InvalidSlot && generation != 0U;
    }

    [[nodiscard]] bool operator==(const AudioVoiceHandle2D&) const noexcept = default;
};

enum class AudioPlaybackState2D : std::uint8_t
{
    Playing = 0,
    Paused,
};

enum class AudioEventType2D : std::uint8_t
{
    Started = 0,
    Paused,
    Resumed,
    Stopped,
    Looped,
    Finished,
    Detached,
};

[[nodiscard]] std::string_view ToString(AudioEventType2D value) noexcept;

enum class AudioEventReason2D : std::uint8_t
{
    None = 0,
    Command,
    EntityDestroyed,
    ResourceUnavailable,
};

[[nodiscard]] std::string_view ToString(AudioEventReason2D value) noexcept;

enum class AudioCommandResult2D : std::uint8_t
{
    Success = 0,
    EntityNotFound,
    SourceMissing,
    SourceInvalid,
    ClipNotReady,
    InvalidVoice,
    StaleVoice,
    InvalidState,
    InvalidInput,
    CapacityExceeded,
};

[[nodiscard]] std::string_view ToString(AudioCommandResult2D value) noexcept;

enum class AudioStepResult2D : std::uint8_t
{
    Success = 0,
    InvalidDelta,
    CapacityExceeded,
};

[[nodiscard]] std::string_view ToString(AudioStepResult2D value) noexcept;

enum class AudioAutoplayResult2D : std::uint8_t
{
    Success = 0,
    AlreadyStarted,
    SourceInvalid,
    ClipNotReady,
    CapacityExceeded,
};

[[nodiscard]] std::string_view ToString(AudioAutoplayResult2D value) noexcept;

struct AudioPlayResult2D final
{
    AudioCommandResult2D result{AudioCommandResult2D::Success};
    AudioVoiceHandle2D voice{};
};

struct AudioVoiceSnapshot2D final
{
    scene::EntityId entity{};
    assets::ResourceHandle<assets::AudioClipResource> clip{};
    AudioGroup2D group{AudioGroup2D::Sfx};
    AudioPlaybackState2D state{AudioPlaybackState2D::Playing};
    float sourceVolume{1.0F};
    float effectiveVolume{1.0F};
    float pitch{1.0F};
    bool loop{false};
    double positionFrames{0.0};
    std::uint64_t completedLoops{0U};
};

struct AudioEvent2D final
{
    std::uint64_t sequence{0U};
    AudioEventType2D type{AudioEventType2D::Started};
    AudioEventReason2D reason{AudioEventReason2D::None};
    AudioVoiceHandle2D voice{};
    scene::EntityId entity{};
    assets::ResourceHandle<assets::AudioClipResource> clip{};
    std::uint64_t loopsCrossed{0U};
};

struct AudioStepReport2D final
{
    AudioStepResult2D result{AudioStepResult2D::Success};
    std::size_t generatedEventCount{0U};
    std::size_t requiredEventCapacity{0U};
};

struct AudioAutoplayReport2D final
{
    AudioAutoplayResult2D result{AudioAutoplayResult2D::Success};
    std::size_t startedVoiceCount{0U};
    std::size_t requiredVoiceCapacity{0U};
    std::size_t requiredEventCapacity{0U};
};

struct AudioMetrics2D final
{
    std::size_t retainedVoiceCapacity{0U};
    std::size_t retainedEventCapacity{0U};
    std::size_t allocatedVoiceSlots{0U};
    std::size_t activeVoiceCount{0U};
    std::size_t voiceHighWatermark{0U};
    std::size_t publishedEventCount{0U};
    std::uint64_t commandCount{0U};
    std::uint64_t commandFailureCount{0U};
    std::uint64_t stepCount{0U};
    std::uint64_t loopEventCount{0U};
    std::uint64_t completionEventCount{0U};
    std::uint64_t detachedVoiceCount{0U};
    std::uint64_t eventCapacityFailureCount{0U};
};

class AudioSystem2D final
{
public:
    AudioSystem2D(
        scene::Scene& scene,
        assets::ResourceRegistry& resources,
        scene::ComponentTypeHandle<AudioSource2D> sourceType) noexcept;

    AudioSystem2D(const AudioSystem2D&) = delete;
    AudioSystem2D& operator=(const AudioSystem2D&) = delete;
    AudioSystem2D(AudioSystem2D&&) = delete;
    AudioSystem2D& operator=(AudioSystem2D&&) = delete;
    ~AudioSystem2D() = default;

    [[nodiscard]] bool ReserveVoices(std::size_t capacity);
    [[nodiscard]] bool ReserveEvents(std::size_t capacity);

    [[nodiscard]] AudioPlayResult2D Play(scene::EntityId entity);
    [[nodiscard]] AudioCommandResult2D Pause(AudioVoiceHandle2D voice);
    [[nodiscard]] AudioCommandResult2D Resume(AudioVoiceHandle2D voice);
    [[nodiscard]] AudioCommandResult2D Stop(AudioVoiceHandle2D voice);
    [[nodiscard]] AudioCommandResult2D SetGroupVolume(AudioGroup2D group, float volume) noexcept;
    [[nodiscard]] AudioAutoplayReport2D StartAutoplay();

    [[nodiscard]] AudioStepReport2D Step(std::chrono::nanoseconds delta);

    void ClearEvents() noexcept;
    [[nodiscard]] std::span<const AudioEvent2D> Events() const noexcept;
    [[nodiscard]] std::optional<AudioVoiceSnapshot2D> InspectVoice(AudioVoiceHandle2D voice) const noexcept;
    [[nodiscard]] float GroupVolume(AudioGroup2D group) const noexcept;
    [[nodiscard]] AudioMetrics2D Metrics() const noexcept;

private:
    struct VoiceData final
    {
        scene::EntityId entity{};
        assets::ResourceHandle<assets::AudioClipResource> clip{};
        AudioGroup2D group{AudioGroup2D::Sfx};
        AudioPlaybackState2D state{AudioPlaybackState2D::Playing};
        float sourceVolume{1.0F};
        float pitch{1.0F};
        bool loop{false};
        double positionFrames{0.0};
        std::uint64_t completedLoops{0U};
    };

    struct VoiceSlot final
    {
        std::uint32_t generation{1U};
        bool active{false};
        VoiceData data{};
    };

    [[nodiscard]] AudioPlayResult2D PlaySource(
        scene::EntityId entity,
        const AudioSource2D& source,
        assets::ResourceHandle<assets::AudioClipResource> clip,
        bool countCommand);
    [[nodiscard]] AudioCommandResult2D ValidateVoice(
        AudioVoiceHandle2D voice,
        VoiceSlot*& outSlot) noexcept;
    [[nodiscard]] AudioCommandResult2D ValidateVoice(
        AudioVoiceHandle2D voice,
        const VoiceSlot*& outSlot) const noexcept;
    [[nodiscard]] std::uint32_t AllocateVoiceSlot();
    void DeactivateVoice(std::uint32_t slotIndex) noexcept;
    void PublishEvent(
        AudioEventType2D type,
        AudioEventReason2D reason,
        AudioVoiceHandle2D voice,
        const VoiceData& data,
        std::uint64_t loopsCrossed = 0U) noexcept;
    [[nodiscard]] float EffectiveVolume(const VoiceData& data) const noexcept;
    [[nodiscard]] static std::uint32_t NextGeneration(std::uint32_t generation) noexcept;
    [[nodiscard]] static std::size_t GroupIndex(AudioGroup2D group) noexcept;

    scene::Scene& scene_;
    assets::ResourceRegistry& resources_;
    scene::ComponentTypeHandle<AudioSource2D> sourceType_{};
    std::vector<VoiceSlot> voices_{};
    std::vector<std::uint32_t> freeVoiceSlots_{};
    std::vector<AudioEvent2D> events_{};
    std::array<float, AudioGroupCount2D> groupVolumes_{1.0F, 1.0F, 1.0F, 1.0F};
    std::size_t voiceCapacity_{0U};
    std::size_t eventCapacity_{0U};
    std::size_t activeVoiceCount_{0U};
    std::size_t voiceHighWatermark_{0U};
    std::uint64_t nextEventSequence_{0U};
    std::uint64_t commandCount_{0U};
    std::uint64_t commandFailureCount_{0U};
    std::uint64_t stepCount_{0U};
    std::uint64_t loopEventCount_{0U};
    std::uint64_t completionEventCount_{0U};
    std::uint64_t detachedVoiceCount_{0U};
    std::uint64_t eventCapacityFailureCount_{0U};
    bool autoplayStarted_{false};
};
} // namespace trace2d::audio
