#include <trace2d/agent/Inspection.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace trace2d::agent
{
namespace
{
[[nodiscard]] SpriteAnimationInspectionError MakeError(
    const SpriteAnimationInspectionErrorCode code,
    std::string message,
    const runtime::SpriteAnimator2DError runtimeError = runtime::SpriteAnimator2DError::None)
{
    return SpriteAnimationInspectionError{
        .code = code,
        .runtimeError = runtimeError,
        .message = std::move(message),
    };
}

[[nodiscard]] std::optional<SpriteAnimationInspectionError> ValidateBinding(
    const SpriteAnimatorBinding& binding)
{
    if (binding.animator == nullptr)
    {
        return MakeError(
            SpriteAnimationInspectionErrorCode::AnimatorUnavailable,
            "No SpriteAnimator2D is bound for Agent verification.");
    }
    if (!binding.animator->HasState())
    {
        return MakeError(
            SpriteAnimationInspectionErrorCode::AnimatorStateUnavailable,
            "The bound SpriteAnimator2D has no authoritative state.");
    }

    const runtime::SpriteAnimator2DState& state = binding.animator->State();
    if (state.clip == nullptr || !state.clip->Prepared())
    {
        return MakeError(
            SpriteAnimationInspectionErrorCode::ClipUnavailable,
            "The bound SpriteAnimator2D does not reference a prepared clip.");
    }
    return std::nullopt;
}

[[nodiscard]] SpriteAnimatorSnapshot MakeSnapshot(const SpriteAnimatorBinding& binding)
{
    const runtime::SpriteAnimator2DState& state = binding.animator->State();
    const runtime::SpriteAnimationClip2D& clip = *state.clip;
    std::uint32_t regionIndex = 0U;
    (void)binding.animator->TryGetCurrentRegionIndex(regionIndex);

    return SpriteAnimatorSnapshot{
        .entitySemanticId = std::string{binding.entitySemanticId},
        .clipDurationNanoseconds = clip.Duration().count(),
        .clipFrameCount = clip.FrameCount(),
        .clipEventCount = clip.EventCount(),
        .timeNanoseconds = state.time.count(),
        .frameIndex = state.frameIndex,
        .regionIndex = regionIndex,
        .playback = state.playback,
        .loopMode = state.loopMode,
        .direction = state.direction,
        .completed = state.completed,
        .speedNumerator = state.speed.numerator,
        .speedDenominator = state.speed.denominator,
        .speedRemainder = state.speedRemainder,
    };
}

[[nodiscard]] SpriteAnimationAssertionContext MakeAssertionContext(
    const SpriteAnimatorSnapshot& snapshot)
{
    return SpriteAnimationAssertionContext{
        .entitySemanticId = snapshot.entitySemanticId,
        .timeNanoseconds = snapshot.timeNanoseconds,
        .frameIndex = snapshot.frameIndex,
        .regionIndex = snapshot.regionIndex,
        .playback = snapshot.playback,
        .loopMode = snapshot.loopMode,
        .direction = snapshot.direction,
        .completed = snapshot.completed,
    };
}

[[nodiscard]] SpriteAnimationValue ReadAssertionValue(
    const SpriteAnimatorSnapshot& snapshot,
    const SpriteAnimationAssertionField field)
{
    switch (field)
    {
    case SpriteAnimationAssertionField::ClipDurationNanoseconds:
        return SpriteAnimationValue::Signed(snapshot.clipDurationNanoseconds);
    case SpriteAnimationAssertionField::ClipFrameCount:
        return SpriteAnimationValue::Unsigned(snapshot.clipFrameCount);
    case SpriteAnimationAssertionField::ClipEventCount:
        return SpriteAnimationValue::Unsigned(snapshot.clipEventCount);
    case SpriteAnimationAssertionField::TimeNanoseconds:
        return SpriteAnimationValue::Signed(snapshot.timeNanoseconds);
    case SpriteAnimationAssertionField::FrameIndex:
        return SpriteAnimationValue::Unsigned(snapshot.frameIndex);
    case SpriteAnimationAssertionField::RegionIndex:
        return SpriteAnimationValue::Unsigned(snapshot.regionIndex);
    case SpriteAnimationAssertionField::Playback:
        return SpriteAnimationValue::String(std::string{ToString(snapshot.playback)});
    case SpriteAnimationAssertionField::LoopMode:
        return SpriteAnimationValue::String(std::string{ToString(snapshot.loopMode)});
    case SpriteAnimationAssertionField::Direction:
        return SpriteAnimationValue::String(std::string{ToString(snapshot.direction)});
    case SpriteAnimationAssertionField::Completed:
        return SpriteAnimationValue::Boolean(snapshot.completed);
    case SpriteAnimationAssertionField::SpeedNumerator:
        return SpriteAnimationValue::Unsigned(snapshot.speedNumerator);
    case SpriteAnimationAssertionField::SpeedDenominator:
        return SpriteAnimationValue::Unsigned(snapshot.speedDenominator);
    case SpriteAnimationAssertionField::SpeedRemainder:
        return SpriteAnimationValue::Unsigned(snapshot.speedRemainder);
    }
    return {};
}

[[nodiscard]] bool ValuesEqual(
    const SpriteAnimationValue& expected,
    const SpriteAnimationValue& observed) noexcept
{
    if (expected.kind != observed.kind) return false;

    switch (expected.kind)
    {
    case SpriteAnimationValueKind::Boolean:
        return expected.booleanValue == observed.booleanValue;
    case SpriteAnimationValueKind::SignedInteger:
        return expected.signedIntegerValue == observed.signedIntegerValue;
    case SpriteAnimationValueKind::UnsignedInteger:
        return expected.unsignedIntegerValue == observed.unsignedIntegerValue;
    case SpriteAnimationValueKind::String:
        return expected.stringValue == observed.stringValue;
    }
    return false;
}

[[nodiscard]] SpriteAnimationInspectionError RuntimeError(
    const runtime::SpriteAnimator2DError error)
{
    if (error == runtime::SpriteAnimator2DError::OutputCapacityExceeded)
    {
        return MakeError(
            SpriteAnimationInspectionErrorCode::OutputCapacityExceeded,
            "Sprite animation advance exceeded the explicit Agent emission capacity; animator state was not committed.",
            error);
    }
    return MakeError(
        SpriteAnimationInspectionErrorCode::RuntimeRejected,
        std::string{"SpriteAnimator2D rejected the Agent action: "} + std::string{ToString(error)} + ".",
        error);
}
} // namespace

std::string_view ToString(const SpriteAnimationInspectionErrorCode code) noexcept
{
    switch (code)
    {
    case SpriteAnimationInspectionErrorCode::AnimatorUnavailable: return "animator_unavailable";
    case SpriteAnimationInspectionErrorCode::AnimatorStateUnavailable: return "animator_state_unavailable";
    case SpriteAnimationInspectionErrorCode::ClipUnavailable: return "clip_unavailable";
    case SpriteAnimationInspectionErrorCode::InvalidAction: return "invalid_action";
    case SpriteAnimationInspectionErrorCode::InvalidAssertion: return "invalid_assertion";
    case SpriteAnimationInspectionErrorCode::TypeMismatch: return "type_mismatch";
    case SpriteAnimationInspectionErrorCode::StateMismatch: return "state_mismatch";
    case SpriteAnimationInspectionErrorCode::RuntimeRejected: return "runtime_rejected";
    case SpriteAnimationInspectionErrorCode::OutputCapacityExceeded: return "output_capacity_exceeded";
    }
    return "unknown_sprite_animation_inspection_error";
}

std::string_view ToString(const runtime::SpriteAnimationPlaybackState state) noexcept
{
    switch (state)
    {
    case runtime::SpriteAnimationPlaybackState::Stopped: return "stopped";
    case runtime::SpriteAnimationPlaybackState::Playing: return "playing";
    case runtime::SpriteAnimationPlaybackState::Paused: return "paused";
    }
    return "unknown";
}

std::string_view ToString(const runtime::SpriteAnimationLoopMode mode) noexcept
{
    switch (mode)
    {
    case runtime::SpriteAnimationLoopMode::Once: return "once";
    case runtime::SpriteAnimationLoopMode::Loop: return "loop";
    case runtime::SpriteAnimationLoopMode::PingPong: return "ping_pong";
    }
    return "unknown";
}

std::string_view ToString(const runtime::SpriteAnimationDirection direction) noexcept
{
    switch (direction)
    {
    case runtime::SpriteAnimationDirection::Forward: return "forward";
    case runtime::SpriteAnimationDirection::Reverse: return "reverse";
    }
    return "unknown";
}

std::string_view ToString(const runtime::SpriteAnimationEmissionKind kind) noexcept
{
    switch (kind)
    {
    case runtime::SpriteAnimationEmissionKind::AuthoredEvent: return "authored_event";
    case runtime::SpriteAnimationEmissionKind::Loop: return "loop";
    case runtime::SpriteAnimationEmissionKind::Bounce: return "bounce";
    case runtime::SpriteAnimationEmissionKind::Completed: return "completed";
    }
    return "unknown";
}

std::string_view ToString(const runtime::SpriteAnimator2DError error) noexcept
{
    switch (error)
    {
    case runtime::SpriteAnimator2DError::None: return "none";
    case runtime::SpriteAnimator2DError::NoState: return "no_state";
    case runtime::SpriteAnimator2DError::NullClip: return "null_clip";
    case runtime::SpriteAnimator2DError::UnpreparedClip: return "unprepared_clip";
    case runtime::SpriteAnimator2DError::TimeOutOfRange: return "time_out_of_range";
    case runtime::SpriteAnimator2DError::FrameIndexMismatch: return "frame_index_mismatch";
    case runtime::SpriteAnimator2DError::InvalidPlaybackState: return "invalid_playback_state";
    case runtime::SpriteAnimator2DError::InvalidLoopMode: return "invalid_loop_mode";
    case runtime::SpriteAnimator2DError::InvalidDirection: return "invalid_direction";
    case runtime::SpriteAnimator2DError::InvalidSpeed: return "invalid_speed";
    case runtime::SpriteAnimator2DError::InvalidSpeedRemainder: return "invalid_speed_remainder";
    case runtime::SpriteAnimator2DError::InvalidCompletionState: return "invalid_completion_state";
    case runtime::SpriteAnimator2DError::InvalidPlaybackTransition: return "invalid_playback_transition";
    case runtime::SpriteAnimator2DError::NegativeDelta: return "negative_delta";
    case runtime::SpriteAnimator2DError::AdvanceOverflow: return "advance_overflow";
    case runtime::SpriteAnimator2DError::OutputCapacityExceeded: return "output_capacity_exceeded";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAnimationValueKind kind) noexcept
{
    switch (kind)
    {
    case SpriteAnimationValueKind::Boolean: return "bool";
    case SpriteAnimationValueKind::SignedInteger: return "int64";
    case SpriteAnimationValueKind::UnsignedInteger: return "uint64";
    case SpriteAnimationValueKind::String: return "string";
    }
    return "unknown";
}

SpriteAnimationValue SpriteAnimationValue::Boolean(const bool value) noexcept
{
    SpriteAnimationValue result{};
    result.kind = SpriteAnimationValueKind::Boolean;
    result.booleanValue = value;
    return result;
}

SpriteAnimationValue SpriteAnimationValue::Signed(const std::int64_t value) noexcept
{
    SpriteAnimationValue result{};
    result.kind = SpriteAnimationValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

SpriteAnimationValue SpriteAnimationValue::Unsigned(const std::uint64_t value) noexcept
{
    SpriteAnimationValue result{};
    result.kind = SpriteAnimationValueKind::UnsignedInteger;
    result.unsignedIntegerValue = value;
    return result;
}

SpriteAnimationValue SpriteAnimationValue::String(std::string value) noexcept
{
    SpriteAnimationValue result{};
    result.kind = SpriteAnimationValueKind::String;
    result.stringValue = std::move(value);
    return result;
}

std::string_view ToString(const SpriteAnimationActionKind kind) noexcept
{
    switch (kind)
    {
    case SpriteAnimationActionKind::Play: return "play";
    case SpriteAnimationActionKind::Pause: return "pause";
    case SpriteAnimationActionKind::Stop: return "stop";
    case SpriteAnimationActionKind::Reset: return "reset";
    case SpriteAnimationActionKind::Restart: return "restart";
    case SpriteAnimationActionKind::Seek: return "seek";
    case SpriteAnimationActionKind::SetSpeed: return "set_speed";
    case SpriteAnimationActionKind::SetDirection: return "set_direction";
    case SpriteAnimationActionKind::Advance: return "advance";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAnimationAssertionField field) noexcept
{
    switch (field)
    {
    case SpriteAnimationAssertionField::ClipDurationNanoseconds: return "clip_duration_ns";
    case SpriteAnimationAssertionField::ClipFrameCount: return "clip_frame_count";
    case SpriteAnimationAssertionField::ClipEventCount: return "clip_event_count";
    case SpriteAnimationAssertionField::TimeNanoseconds: return "time_ns";
    case SpriteAnimationAssertionField::FrameIndex: return "frame_index";
    case SpriteAnimationAssertionField::RegionIndex: return "region_index";
    case SpriteAnimationAssertionField::Playback: return "playback";
    case SpriteAnimationAssertionField::LoopMode: return "loop_mode";
    case SpriteAnimationAssertionField::Direction: return "direction";
    case SpriteAnimationAssertionField::Completed: return "completed";
    case SpriteAnimationAssertionField::SpeedNumerator: return "speed_numerator";
    case SpriteAnimationAssertionField::SpeedDenominator: return "speed_denominator";
    case SpriteAnimationAssertionField::SpeedRemainder: return "speed_remainder";
    }
    return "unknown";
}

SpriteAnimatorInspectionResult AgentFacade::InspectSpriteAnimator(
    const SpriteAnimatorBinding& binding) const
{
    if (const auto error = ValidateBinding(binding); error.has_value())
    {
        return {.snapshot = std::nullopt, .error = *error};
    }
    return {.snapshot = MakeSnapshot(binding), .error = std::nullopt};
}

SpriteAnimationActionResult AgentFacade::ActOnSpriteAnimator(
    const SpriteAnimatorBinding& binding,
    const SpriteAnimationAction& action)
{
    SpriteAnimationActionResult response{.action = action};
    if (const auto error = ValidateBinding(binding); error.has_value())
    {
        response.error = *error;
        return response;
    }

    if (action.kind == SpriteAnimationActionKind::Advance
        && action.emissionCapacity > MaxSpriteAnimationAgentEmissions)
    {
        response.snapshot = MakeSnapshot(binding);
        response.error = MakeError(
            SpriteAnimationInspectionErrorCode::InvalidAction,
            "Sprite animation emission_capacity exceeds the bounded Agent maximum of 4096.");
        return response;
    }

    runtime::SpriteAnimator2DStatus status{};
    runtime::SpriteAnimationAdvanceResult2D advanceResult{};
    bool usedAdvance = false;

    switch (action.kind)
    {
    case SpriteAnimationActionKind::Play:
        status = binding.animator->Play();
        break;
    case SpriteAnimationActionKind::Pause:
        status = binding.animator->Pause();
        break;
    case SpriteAnimationActionKind::Stop:
        status = binding.animator->Stop();
        break;
    case SpriteAnimationActionKind::Reset:
        status = binding.animator->Reset();
        break;
    case SpriteAnimationActionKind::Restart:
        status = binding.animator->Restart();
        break;
    case SpriteAnimationActionKind::Seek:
        status = binding.animator->Seek(action.time);
        break;
    case SpriteAnimationActionKind::SetSpeed:
        status = binding.animator->SetSpeed(action.speed);
        break;
    case SpriteAnimationActionKind::SetDirection:
        status = binding.animator->SetDirection(action.direction);
        break;
    case SpriteAnimationActionKind::Advance:
    {
        usedAdvance = true;
        std::vector<runtime::SpriteAnimationEmission2D> output(action.emissionCapacity);
        advanceResult = binding.animator->Advance(action.time, output);
        if (advanceResult.Succeeded())
        {
            response.emissions.reserve(advanceResult.emissionCount);
            for (std::size_t index = 0U; index < advanceResult.emissionCount; ++index)
            {
                const runtime::SpriteAnimationEmission2D& emission = output[index];
                response.emissions.push_back(SpriteAnimationEmissionSnapshot{
                    .kind = emission.kind,
                    .eventId = emission.eventId,
                    .authoredOrdinal = emission.authoredOrdinal,
                    .timeNanoseconds = emission.time.count(),
                    .direction = emission.direction,
                });
            }
        }
        break;
    }
    }

    const runtime::SpriteAnimator2DError runtimeError = usedAdvance ? advanceResult.error : status.error;
    response.snapshot = MakeSnapshot(binding);
    if (runtimeError != runtime::SpriteAnimator2DError::None)
    {
        response.error = RuntimeError(runtimeError);
        response.emissions.clear();
        return response;
    }
    return response;
}

SpriteAnimationAssertionResult AgentFacade::AssertSpriteAnimator(
    const SpriteAnimatorBinding& binding,
    const SpriteAnimationAssertion& assertion) const
{
    SpriteAnimationAssertionResult response{.assertion = assertion};
    if (const auto error = ValidateBinding(binding); error.has_value())
    {
        response.error = *error;
        return response;
    }

    const SpriteAnimatorSnapshot snapshot = MakeSnapshot(binding);
    response.context = MakeAssertionContext(snapshot);
    response.observed = ReadAssertionValue(snapshot, assertion.field);

    if (response.observed->kind != assertion.expected.kind)
    {
        response.error = MakeError(
            SpriteAnimationInspectionErrorCode::TypeMismatch,
            "Sprite animation assertion expected value kind does not match the selected field.");
        return response;
    }
    if (!ValuesEqual(assertion.expected, *response.observed))
    {
        response.error = MakeError(
            SpriteAnimationInspectionErrorCode::StateMismatch,
            "Sprite animation authoritative state does not match the exact expected value.");
    }
    return response;
}
} // namespace trace2d::agent
