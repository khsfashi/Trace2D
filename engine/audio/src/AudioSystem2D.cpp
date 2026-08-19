#include <trace2d/audio/AudioSystem2D.hpp>

#include <cmath>
#include <limits>
#include <string>

namespace trace2d::audio
{
namespace
{
struct AdvancePrediction final
{
    bool finished{false};
    std::uint64_t loopsCrossed{0U};
    double positionFrames{0.0};
};

[[nodiscard]] AdvancePrediction PredictAdvance(
    const double currentPositionFrames,
    const float pitch,
    const bool loop,
    const assets::AudioClipResource& clip,
    const std::chrono::nanoseconds delta) noexcept
{
    const long double seconds = static_cast<long double>(delta.count()) / 1'000'000'000.0L;
    const long double advance = seconds * static_cast<long double>(clip.sampleRateHz) *
                                static_cast<long double>(pitch);
    const long double frameCount = static_cast<long double>(clip.frameCount);
    const long double total = static_cast<long double>(currentPositionFrames) + advance;

    AdvancePrediction prediction{};
    if (!loop)
    {
        if (total >= frameCount)
        {
            prediction.finished = true;
            prediction.positionFrames = static_cast<double>(frameCount);
        }
        else
        {
            prediction.positionFrames = static_cast<double>(total);
        }
        return prediction;
    }

    const long double loopCount = std::floor(total / frameCount);
    prediction.loopsCrossed = static_cast<std::uint64_t>(loopCount);
    prediction.positionFrames = static_cast<double>(std::fmod(total, frameCount));
    return prediction;
}

[[nodiscard]] std::size_t SaturatingAdd(const std::size_t left, const std::size_t right) noexcept
{
    if (right > std::numeric_limits<std::size_t>::max() - left)
    {
        return std::numeric_limits<std::size_t>::max();
    }
    return left + right;
}
} // namespace

std::string_view ToString(const AudioEventType2D value) noexcept
{
    switch (value)
    {
    case AudioEventType2D::Started: return "started";
    case AudioEventType2D::Paused: return "paused";
    case AudioEventType2D::Resumed: return "resumed";
    case AudioEventType2D::Stopped: return "stopped";
    case AudioEventType2D::Looped: return "looped";
    case AudioEventType2D::Finished: return "finished";
    case AudioEventType2D::Detached: return "detached";
    }
    return "unknown";
}

std::string_view ToString(const AudioEventReason2D value) noexcept
{
    switch (value)
    {
    case AudioEventReason2D::None: return "none";
    case AudioEventReason2D::Command: return "command";
    case AudioEventReason2D::EntityDestroyed: return "entity_destroyed";
    case AudioEventReason2D::ResourceUnavailable: return "resource_unavailable";
    }
    return "unknown";
}

std::string_view ToString(const AudioCommandResult2D value) noexcept
{
    switch (value)
    {
    case AudioCommandResult2D::Success: return "success";
    case AudioCommandResult2D::EntityNotFound: return "entity_not_found";
    case AudioCommandResult2D::SourceMissing: return "source_missing";
    case AudioCommandResult2D::SourceInvalid: return "source_invalid";
    case AudioCommandResult2D::ClipNotReady: return "clip_not_ready";
    case AudioCommandResult2D::InvalidVoice: return "invalid_voice";
    case AudioCommandResult2D::StaleVoice: return "stale_voice";
    case AudioCommandResult2D::InvalidState: return "invalid_state";
    case AudioCommandResult2D::InvalidInput: return "invalid_input";
    case AudioCommandResult2D::CapacityExceeded: return "capacity_exceeded";
    }
    return "unknown";
}

std::string_view ToString(const AudioStepResult2D value) noexcept
{
    switch (value)
    {
    case AudioStepResult2D::Success: return "success";
    case AudioStepResult2D::InvalidDelta: return "invalid_delta";
    case AudioStepResult2D::CapacityExceeded: return "capacity_exceeded";
    }
    return "unknown";
}

std::string_view ToString(const AudioAutoplayResult2D value) noexcept
{
    switch (value)
    {
    case AudioAutoplayResult2D::Success: return "success";
    case AudioAutoplayResult2D::AlreadyStarted: return "already_started";
    case AudioAutoplayResult2D::SourceInvalid: return "source_invalid";
    case AudioAutoplayResult2D::ClipNotReady: return "clip_not_ready";
    case AudioAutoplayResult2D::CapacityExceeded: return "capacity_exceeded";
    }
    return "unknown";
}

AudioSystem2D::AudioSystem2D(
    scene::Scene& scene,
    assets::ResourceRegistry& resources,
    const scene::ComponentTypeHandle<AudioSource2D> sourceType) noexcept
    : scene_{scene}
    , resources_{resources}
    , sourceType_{sourceType}
{
}

AudioSystem2D::~AudioSystem2D()
{
    for (VoiceSlot& slot : voices_)
    {
        if (slot.active)
        {
            ReleaseVoiceRetention(slot);
        }
    }
}

bool AudioSystem2D::ReserveVoices(const std::size_t capacity)
{
    if (capacity > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }
    if (capacity <= voiceCapacity_)
    {
        return true;
    }
    voices_.reserve(capacity);
    freeVoiceSlots_.reserve(capacity);
    voiceCapacity_ = capacity;
    return true;
}

bool AudioSystem2D::ReserveEvents(const std::size_t capacity)
{
    if (capacity <= eventCapacity_)
    {
        return true;
    }
    events_.reserve(capacity);
    eventCapacity_ = capacity;
    return true;
}

AudioPlayResult2D AudioSystem2D::Play(const scene::EntityId entity)
{
    ++commandCount_;
    if (!scene_.Contains(entity))
    {
        ++commandFailureCount_;
        return {AudioCommandResult2D::EntityNotFound, {}};
    }

    const AudioSource2D* const source = scene_.TryGetComponent(entity, sourceType_);
    if (source == nullptr)
    {
        ++commandFailureCount_;
        return {AudioCommandResult2D::SourceMissing, {}};
    }

    std::string validationError{};
    if (!ValidateAudioSource2D(*source, validationError))
    {
        ++commandFailureCount_;
        return {AudioCommandResult2D::SourceInvalid, {}};
    }

    const std::optional<assets::ResourceHandle<assets::AudioClipResource>> clip =
        resources_.FindReadyAudioClip(source->clipReference);
    if (!clip.has_value() || resources_.Resolve(*clip) == nullptr)
    {
        ++commandFailureCount_;
        return {AudioCommandResult2D::ClipNotReady, {}};
    }

    const AudioPlayResult2D result = PlaySource(entity, *source, *clip, false);
    if (result.result != AudioCommandResult2D::Success)
    {
        ++commandFailureCount_;
    }
    return result;
}

AudioCommandResult2D AudioSystem2D::Pause(const AudioVoiceHandle2D voice)
{
    ++commandCount_;
    VoiceSlot* slot = nullptr;
    const AudioCommandResult2D validation = ValidateVoice(voice, slot);
    if (validation != AudioCommandResult2D::Success)
    {
        ++commandFailureCount_;
        return validation;
    }
    if (slot->data.state != AudioPlaybackState2D::Playing)
    {
        ++commandFailureCount_;
        return AudioCommandResult2D::InvalidState;
    }
    if (events_.size() >= eventCapacity_)
    {
        ++commandFailureCount_;
        ++eventCapacityFailureCount_;
        return AudioCommandResult2D::CapacityExceeded;
    }

    slot->data.state = AudioPlaybackState2D::Paused;
    PublishEvent(AudioEventType2D::Paused, AudioEventReason2D::Command, voice, slot->data);
    return AudioCommandResult2D::Success;
}

AudioCommandResult2D AudioSystem2D::Resume(const AudioVoiceHandle2D voice)
{
    ++commandCount_;
    VoiceSlot* slot = nullptr;
    const AudioCommandResult2D validation = ValidateVoice(voice, slot);
    if (validation != AudioCommandResult2D::Success)
    {
        ++commandFailureCount_;
        return validation;
    }
    if (slot->data.state != AudioPlaybackState2D::Paused)
    {
        ++commandFailureCount_;
        return AudioCommandResult2D::InvalidState;
    }
    if (events_.size() >= eventCapacity_)
    {
        ++commandFailureCount_;
        ++eventCapacityFailureCount_;
        return AudioCommandResult2D::CapacityExceeded;
    }

    slot->data.state = AudioPlaybackState2D::Playing;
    PublishEvent(AudioEventType2D::Resumed, AudioEventReason2D::Command, voice, slot->data);
    return AudioCommandResult2D::Success;
}

AudioCommandResult2D AudioSystem2D::Stop(const AudioVoiceHandle2D voice)
{
    ++commandCount_;
    VoiceSlot* slot = nullptr;
    const AudioCommandResult2D validation = ValidateVoice(voice, slot);
    if (validation != AudioCommandResult2D::Success)
    {
        ++commandFailureCount_;
        return validation;
    }
    if (events_.size() >= eventCapacity_)
    {
        ++commandFailureCount_;
        ++eventCapacityFailureCount_;
        return AudioCommandResult2D::CapacityExceeded;
    }

    PublishEvent(AudioEventType2D::Stopped, AudioEventReason2D::Command, voice, slot->data);
    DeactivateVoice(voice.slot);
    return AudioCommandResult2D::Success;
}

AudioCommandResult2D AudioSystem2D::SetGroupVolume(const AudioGroup2D group, const float volume) noexcept
{
    ++commandCount_;
    if (group >= AudioGroup2D::Count || !std::isfinite(volume) || volume < 0.0F || volume > 1.0F)
    {
        ++commandFailureCount_;
        return AudioCommandResult2D::InvalidInput;
    }
    groupVolumes_[GroupIndex(group)] = volume;
    return AudioCommandResult2D::Success;
}

AudioAutoplayReport2D AudioSystem2D::StartAutoplay()
{
    AudioAutoplayReport2D report{};
    if (autoplayStarted_)
    {
        report.result = AudioAutoplayResult2D::AlreadyStarted;
        return report;
    }

    std::size_t autoplayCount = 0U;
    AudioAutoplayResult2D preflight = AudioAutoplayResult2D::Success;
    scene_.ForEachEntity([&](const scene::EntityId entity, const scene::Entity&)
    {
        if (preflight != AudioAutoplayResult2D::Success)
        {
            return;
        }
        const AudioSource2D* const source = scene_.TryGetComponent(entity, sourceType_);
        if (source == nullptr || !source->autoplay)
        {
            return;
        }

        std::string validationError{};
        if (!ValidateAudioSource2D(*source, validationError))
        {
            preflight = AudioAutoplayResult2D::SourceInvalid;
            return;
        }
        const std::optional<assets::ResourceHandle<assets::AudioClipResource>> clip =
            resources_.FindReadyAudioClip(source->clipReference);
        if (!clip.has_value() || resources_.Resolve(*clip) == nullptr)
        {
            preflight = AudioAutoplayResult2D::ClipNotReady;
            return;
        }
        ++autoplayCount;
    });

    if (preflight != AudioAutoplayResult2D::Success)
    {
        report.result = preflight;
        return report;
    }

    report.requiredVoiceCapacity = SaturatingAdd(activeVoiceCount_, autoplayCount);
    report.requiredEventCapacity = SaturatingAdd(events_.size(), autoplayCount);
    if (report.requiredVoiceCapacity > voiceCapacity_ || report.requiredEventCapacity > eventCapacity_)
    {
        report.result = AudioAutoplayResult2D::CapacityExceeded;
        if (report.requiredEventCapacity > eventCapacity_)
        {
            ++eventCapacityFailureCount_;
        }
        return report;
    }

    AudioAutoplayResult2D startResult = AudioAutoplayResult2D::Success;
    scene_.ForEachEntity([&](const scene::EntityId entity, const scene::Entity&)
    {
        if (startResult != AudioAutoplayResult2D::Success)
        {
            return;
        }
        const AudioSource2D* const source = scene_.TryGetComponent(entity, sourceType_);
        if (source == nullptr || !source->autoplay)
        {
            return;
        }
        const std::optional<assets::ResourceHandle<assets::AudioClipResource>> clip =
            resources_.FindReadyAudioClip(source->clipReference);
        if (!clip.has_value())
        {
            startResult = AudioAutoplayResult2D::ClipNotReady;
            return;
        }
        const AudioPlayResult2D playResult = PlaySource(entity, *source, *clip, false);
        if (playResult.result != AudioCommandResult2D::Success)
        {
            startResult = playResult.result == AudioCommandResult2D::ClipNotReady
                ? AudioAutoplayResult2D::ClipNotReady
                : AudioAutoplayResult2D::SourceInvalid;
            return;
        }
        ++report.startedVoiceCount;
    });

    report.result = startResult;
    if (startResult == AudioAutoplayResult2D::Success)
    {
        autoplayStarted_ = true;
    }
    return report;
}

AudioStepReport2D AudioSystem2D::Step(const std::chrono::nanoseconds delta)
{
    AudioStepReport2D report{};
    if (delta.count() < 0)
    {
        report.result = AudioStepResult2D::InvalidDelta;
        return report;
    }

    std::size_t requiredNewEvents = 0U;
    for (const VoiceSlot& slot : voices_)
    {
        if (!slot.active)
        {
            continue;
        }
        if (!scene_.Contains(slot.data.entity) || resources_.Resolve(slot.data.clip) == nullptr)
        {
            ++requiredNewEvents;
            continue;
        }
        if (slot.data.state == AudioPlaybackState2D::Paused)
        {
            continue;
        }

        const assets::AudioClipResource* const clip = resources_.Resolve(slot.data.clip);
        const AdvancePrediction prediction = PredictAdvance(
            slot.data.positionFrames,
            slot.data.pitch,
            slot.data.loop,
            *clip,
            delta);
        if (prediction.finished || prediction.loopsCrossed != 0U)
        {
            ++requiredNewEvents;
        }
    }

    report.requiredEventCapacity = SaturatingAdd(events_.size(), requiredNewEvents);
    if (report.requiredEventCapacity > eventCapacity_)
    {
        report.result = AudioStepResult2D::CapacityExceeded;
        ++eventCapacityFailureCount_;
        return report;
    }

    const std::size_t eventCountBefore = events_.size();
    for (std::uint32_t slotIndex = 0U; slotIndex < static_cast<std::uint32_t>(voices_.size()); ++slotIndex)
    {
        VoiceSlot& slot = voices_[slotIndex];
        if (!slot.active)
        {
            continue;
        }

        const AudioVoiceHandle2D handle{slotIndex, slot.generation};
        if (!scene_.Contains(slot.data.entity))
        {
            PublishEvent(
                AudioEventType2D::Detached,
                AudioEventReason2D::EntityDestroyed,
                handle,
                slot.data);
            ++detachedVoiceCount_;
            DeactivateVoice(slotIndex);
            continue;
        }

        const assets::AudioClipResource* const clip = resources_.Resolve(slot.data.clip);
        if (clip == nullptr)
        {
            PublishEvent(
                AudioEventType2D::Detached,
                AudioEventReason2D::ResourceUnavailable,
                handle,
                slot.data);
            ++detachedVoiceCount_;
            DeactivateVoice(slotIndex);
            continue;
        }
        if (slot.data.state == AudioPlaybackState2D::Paused)
        {
            continue;
        }

        const AdvancePrediction prediction = PredictAdvance(
            slot.data.positionFrames,
            slot.data.pitch,
            slot.data.loop,
            *clip,
            delta);
        slot.data.positionFrames = prediction.positionFrames;

        if (prediction.finished)
        {
            PublishEvent(AudioEventType2D::Finished, AudioEventReason2D::None, handle, slot.data);
            ++completionEventCount_;
            DeactivateVoice(slotIndex);
            continue;
        }

        if (prediction.loopsCrossed != 0U)
        {
            if (prediction.loopsCrossed > std::numeric_limits<std::uint64_t>::max() - slot.data.completedLoops)
            {
                slot.data.completedLoops = std::numeric_limits<std::uint64_t>::max();
            }
            else
            {
                slot.data.completedLoops += prediction.loopsCrossed;
            }
            PublishEvent(
                AudioEventType2D::Looped,
                AudioEventReason2D::None,
                handle,
                slot.data,
                prediction.loopsCrossed);
            ++loopEventCount_;
        }
    }

    ++stepCount_;
    report.generatedEventCount = events_.size() - eventCountBefore;
    return report;
}

void AudioSystem2D::ClearEvents() noexcept
{
    events_.clear();
}

std::span<const AudioEvent2D> AudioSystem2D::Events() const noexcept
{
    return std::span<const AudioEvent2D>{events_.data(), events_.size()};
}

std::optional<AudioVoiceSnapshot2D> AudioSystem2D::InspectVoice(const AudioVoiceHandle2D voice) const noexcept
{
    const VoiceSlot* slot = nullptr;
    if (ValidateVoice(voice, slot) != AudioCommandResult2D::Success)
    {
        return std::nullopt;
    }
    return AudioVoiceSnapshot2D{
        slot->data.entity,
        slot->data.clip,
        slot->data.group,
        slot->data.state,
        slot->data.sourceVolume,
        EffectiveVolume(slot->data),
        slot->data.pitch,
        slot->data.loop,
        slot->data.positionFrames,
        slot->data.completedLoops};
}

float AudioSystem2D::GroupVolume(const AudioGroup2D group) const noexcept
{
    if (group >= AudioGroup2D::Count)
    {
        return 0.0F;
    }
    return groupVolumes_[GroupIndex(group)];
}

AudioMetrics2D AudioSystem2D::Metrics() const noexcept
{
    return AudioMetrics2D{
        voiceCapacity_,
        eventCapacity_,
        voices_.size(),
        activeVoiceCount_,
        voiceHighWatermark_,
        events_.size(),
        commandCount_,
        commandFailureCount_,
        stepCount_,
        loopEventCount_,
        completionEventCount_,
        detachedVoiceCount_,
        eventCapacityFailureCount_};
}

AudioPlayResult2D AudioSystem2D::PlaySource(
    const scene::EntityId entity,
    const AudioSource2D& source,
    const assets::ResourceHandle<assets::AudioClipResource> clip,
    const bool countCommand)
{
    if (countCommand)
    {
        ++commandCount_;
    }

    std::string validationError{};
    if (!ValidateAudioSource2D(source, validationError) || resources_.Resolve(clip) == nullptr)
    {
        if (countCommand)
        {
            ++commandFailureCount_;
        }
        return {resources_.Resolve(clip) == nullptr ? AudioCommandResult2D::ClipNotReady : AudioCommandResult2D::SourceInvalid, {}};
    }
    if (activeVoiceCount_ >= voiceCapacity_ || events_.size() >= eventCapacity_)
    {
        if (countCommand)
        {
            ++commandFailureCount_;
        }
        if (events_.size() >= eventCapacity_)
        {
            ++eventCapacityFailureCount_;
        }
        return {AudioCommandResult2D::CapacityExceeded, {}};
    }

    const assets::ResourceOperationResult retain = resources_.Retain(clip.Untyped());
    if (!retain.Succeeded())
    {
        if (countCommand)
        {
            ++commandFailureCount_;
        }
        return {AudioCommandResult2D::ClipNotReady, {}};
    }

    const std::uint32_t slotIndex = AllocateVoiceSlot();
    if (slotIndex == AudioVoiceHandle2D::InvalidSlot)
    {
        (void)resources_.Release(clip.Untyped());
        if (countCommand)
        {
            ++commandFailureCount_;
        }
        return {AudioCommandResult2D::CapacityExceeded, {}};
    }

    VoiceSlot& slot = voices_[slotIndex];
    slot.active = true;
    slot.data = VoiceData{
        entity,
        clip,
        source.group,
        AudioPlaybackState2D::Playing,
        source.volume,
        source.pitch,
        source.loop,
        0.0,
        0U};
    ++activeVoiceCount_;
    if (activeVoiceCount_ > voiceHighWatermark_)
    {
        voiceHighWatermark_ = activeVoiceCount_;
    }

    const AudioVoiceHandle2D handle{slotIndex, slot.generation};
    PublishEvent(AudioEventType2D::Started, AudioEventReason2D::None, handle, slot.data);
    return {AudioCommandResult2D::Success, handle};
}

AudioCommandResult2D AudioSystem2D::ValidateVoice(
    const AudioVoiceHandle2D voice,
    VoiceSlot*& outSlot) noexcept
{
    outSlot = nullptr;
    if (!voice.IsValid() || voice.slot >= voices_.size())
    {
        return AudioCommandResult2D::InvalidVoice;
    }
    VoiceSlot& slot = voices_[voice.slot];
    if (!slot.active || slot.generation != voice.generation)
    {
        return AudioCommandResult2D::StaleVoice;
    }
    outSlot = &slot;
    return AudioCommandResult2D::Success;
}

AudioCommandResult2D AudioSystem2D::ValidateVoice(
    const AudioVoiceHandle2D voice,
    const VoiceSlot*& outSlot) const noexcept
{
    outSlot = nullptr;
    if (!voice.IsValid() || voice.slot >= voices_.size())
    {
        return AudioCommandResult2D::InvalidVoice;
    }
    const VoiceSlot& slot = voices_[voice.slot];
    if (!slot.active || slot.generation != voice.generation)
    {
        return AudioCommandResult2D::StaleVoice;
    }
    outSlot = &slot;
    return AudioCommandResult2D::Success;
}

std::uint32_t AudioSystem2D::AllocateVoiceSlot()
{
    if (!freeVoiceSlots_.empty())
    {
        const std::uint32_t slotIndex = freeVoiceSlots_.back();
        freeVoiceSlots_.pop_back();
        return slotIndex;
    }
    if (voices_.size() >= voiceCapacity_ || voices_.size() >= static_cast<std::size_t>(AudioVoiceHandle2D::InvalidSlot))
    {
        return AudioVoiceHandle2D::InvalidSlot;
    }
    voices_.emplace_back();
    return static_cast<std::uint32_t>(voices_.size() - 1U);
}

void AudioSystem2D::ReleaseVoiceRetention(VoiceSlot& slot)
{
    if (resources_.Resolve(slot.data.clip) != nullptr)
    {
        (void)resources_.Release(slot.data.clip.Untyped());
    }
}

void AudioSystem2D::DeactivateVoice(const std::uint32_t slotIndex)
{
    VoiceSlot& slot = voices_[slotIndex];
    ReleaseVoiceRetention(slot);
    slot.active = false;
    slot.data = VoiceData{};
    slot.generation = NextGeneration(slot.generation);
    freeVoiceSlots_.push_back(slotIndex);
    --activeVoiceCount_;
}

void AudioSystem2D::PublishEvent(
    const AudioEventType2D type,
    const AudioEventReason2D reason,
    const AudioVoiceHandle2D voice,
    const VoiceData& data,
    const std::uint64_t loopsCrossed) noexcept
{
    ++nextEventSequence_;
    if (nextEventSequence_ == 0U)
    {
        ++nextEventSequence_;
    }
    events_.push_back(AudioEvent2D{
        nextEventSequence_,
        type,
        reason,
        voice,
        data.entity,
        data.clip,
        loopsCrossed});
}

float AudioSystem2D::EffectiveVolume(const VoiceData& data) const noexcept
{
    const float master = groupVolumes_[GroupIndex(AudioGroup2D::Master)];
    if (data.group == AudioGroup2D::Master)
    {
        return data.sourceVolume * master;
    }
    return data.sourceVolume * master * groupVolumes_[GroupIndex(data.group)];
}

std::uint32_t AudioSystem2D::NextGeneration(std::uint32_t generation) noexcept
{
    ++generation;
    return generation == 0U ? 1U : generation;
}

std::size_t AudioSystem2D::GroupIndex(const AudioGroup2D group) noexcept
{
    return static_cast<std::size_t>(group);
}
} // namespace trace2d::audio
