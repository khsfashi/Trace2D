#pragma once

#include <trace2d/scene/TweenBinding2D.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace trace2d::scene
{
enum class TweenSequencePlaybackState2D : std::uint8_t
{
    Playing = 0,
    Paused,
    Completed,
    Cancelled,
};

enum class TweenSequenceCancellationReason2D : std::uint8_t
{
    None = 0,
    Explicit,
    ChildReplaced,
    TargetInvalidated,
    PropertyWriteRejected,
    BindingFailure,
};

enum class TweenSequenceError2D : std::uint8_t
{
    None = 0,
    InvalidHandle,
    InvalidTimeDomain,
    NegativeDelta,
    InvalidAuthoringMode,
    InvalidTweenSpec,
    ValueTypeMismatch,
    NegativeOffset,
    NegativeInterval,
    MixedTimeDomain,
    InfiniteChildUnsupported,
    TimeOverflow,
    EmptyDefinition,
    ChildConflict,
    BindingFailure,
    InvalidPlaybackTransition,
    SlotCapacityExceeded,
};

struct TweenSequenceStatus2D final
{
    TweenSequenceError2D error{TweenSequenceError2D::None};
    TweenBindingError2D bindingError{TweenBindingError2D::None};
    runtime::Tween2DError tweenError{runtime::Tween2DError::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == TweenSequenceError2D::None;
    }

    [[nodiscard]] bool operator==(const TweenSequenceStatus2D&) const noexcept = default;
};

struct TweenSequenceHandle2D final
{
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{InvalidIndex};
    std::uint64_t generation{0U};

    [[nodiscard]] bool Valid() const noexcept
    {
        return index != InvalidIndex && generation != 0U;
    }

    [[nodiscard]] bool operator==(const TweenSequenceHandle2D&) const noexcept = default;
};

struct TweenSequenceState2D final
{
    runtime::TweenTimeDomain2D domain{runtime::TweenTimeDomain2D::Presentation};
    TweenSequencePlaybackState2D playback{TweenSequencePlaybackState2D::Playing};
    TweenSequenceCancellationReason2D cancellationReason{TweenSequenceCancellationReason2D::None};
    runtime::TweenTime2D elapsed{0};
    runtime::TweenTime2D duration{0};
    std::uint64_t activeChildCount{0U};
    std::uint64_t completedChildCount{0U};
    std::uint64_t scheduledChildCount{0U};

    [[nodiscard]] bool operator==(const TweenSequenceState2D&) const noexcept = default;
};

struct TweenSequenceMetrics2D final
{
    std::uint64_t activeSequenceCount{0U};
    std::uint64_t retainedSequenceSlotCount{0U};
    std::uint64_t retainedSequenceCapacity{0U};
    std::uint64_t retainedChildSlotCount{0U};
    std::uint64_t retainedChildCapacity{0U};
    std::uint64_t highWaterActiveSequenceCount{0U};
    std::uint64_t createdSequenceCount{0U};
    std::uint64_t reusedSequenceSlotCount{0U};

    [[nodiscard]] bool operator==(const TweenSequenceMetrics2D&) const noexcept = default;
};

struct TweenSequenceChildDefinition2D final
{
    runtime::TweenTime2D offset{0};
    ResolvedTweenBinding2D binding{};
    TweenBindingSpec2D bindingSpec{};
    std::uint64_t authorOrder{0U};
};

class TweenSequenceDefinition2D final
{
public:
    explicit TweenSequenceDefinition2D(runtime::TweenTimeDomain2D domain) noexcept;

    [[nodiscard]] TweenSequenceStatus2D Append(
        const ResolvedTweenBinding2D& binding,
        const TweenBindingSpec2D& spec);
    [[nodiscard]] TweenSequenceStatus2D Join(
        const ResolvedTweenBinding2D& binding,
        const TweenBindingSpec2D& spec);
    [[nodiscard]] TweenSequenceStatus2D Insert(
        runtime::TweenTime2D offset,
        const ResolvedTweenBinding2D& binding,
        const TweenBindingSpec2D& spec);
    [[nodiscard]] TweenSequenceStatus2D Interval(runtime::TweenTime2D duration) noexcept;

    [[nodiscard]] runtime::TweenTimeDomain2D Domain() const noexcept { return domain_; }
    [[nodiscard]] runtime::TweenTime2D Duration() const noexcept { return duration_; }
    [[nodiscard]] std::size_t ChildCount() const noexcept { return children_.size(); }

private:
    [[nodiscard]] TweenSequenceStatus2D AddChild(
        runtime::TweenTime2D offset,
        const ResolvedTweenBinding2D& binding,
        const TweenBindingSpec2D& spec);

    runtime::TweenTimeDomain2D domain_{runtime::TweenTimeDomain2D::Presentation};
    runtime::TweenTime2D duration_{0};
    runtime::TweenTime2D joinAnchor_{0};
    std::vector<TweenSequenceChildDefinition2D> children_{};
    std::uint64_t nextAuthorOrder_{0U};

    friend class TweenSequenceSystem2D;
};

class TweenSequenceSystem2D final
{
public:
    explicit TweenSequenceSystem2D(TweenBindingSystem2D& bindings) noexcept;

    void Reserve(std::size_t sequenceCapacity, std::size_t childCapacityPerSequence = 0U);

    [[nodiscard]] TweenSequenceStatus2D Create(
        const TweenSequenceDefinition2D& definition,
        TweenSequenceHandle2D& outHandle);
    [[nodiscard]] TweenSequenceStatus2D Inspect(
        TweenSequenceHandle2D handle,
        TweenSequenceState2D& outState) const noexcept;
    [[nodiscard]] TweenSequenceStatus2D Pause(TweenSequenceHandle2D handle) noexcept;
    [[nodiscard]] TweenSequenceStatus2D Resume(TweenSequenceHandle2D handle) noexcept;
    [[nodiscard]] TweenSequenceStatus2D Restart(TweenSequenceHandle2D handle) noexcept;
    [[nodiscard]] TweenSequenceStatus2D Cancel(TweenSequenceHandle2D handle) noexcept;
    // SequenceSystem owns domain stepping while sequences are active: this advances the shared
    // TweenBindingSystem exactly once per supplied delta, including non-sequence tweens. Callers
    // must not also step the same binding system for the same domain tick.
    [[nodiscard]] TweenSequenceStatus2D Step(
        runtime::TweenTimeDomain2D domain,
        runtime::TweenTime2D delta) noexcept;

    [[nodiscard]] TweenSequenceMetrics2D Metrics() const noexcept;

private:
    struct Slot final
    {
        runtime::TweenTimeDomain2D domain{runtime::TweenTimeDomain2D::Presentation};
        runtime::TweenTime2D duration{0};
        runtime::TweenTime2D elapsed{0};
        TweenSequencePlaybackState2D playback{TweenSequencePlaybackState2D::Playing};
        TweenSequenceCancellationReason2D cancellationReason{TweenSequenceCancellationReason2D::None};
        std::vector<TweenSequenceChildDefinition2D> children{};
        std::vector<runtime::TweenHandle2D> childHandles{};
        std::size_t nextChildIndex{0U};
        std::uint64_t generation{0U};
        bool occupied{false};
    };

    [[nodiscard]] Slot* ResolveMutable(TweenSequenceHandle2D handle) noexcept;
    [[nodiscard]] const Slot* Resolve(TweenSequenceHandle2D handle) const noexcept;
    [[nodiscard]] TweenSequenceStatus2D ValidateDefinition(
        const TweenSequenceDefinition2D& definition) const noexcept;
    [[nodiscard]] TweenSequenceStatus2D ActivateDueChildren(Slot& slot) noexcept;
    [[nodiscard]] TweenSequenceStatus2D RefreshChildState(Slot& slot) noexcept;
    [[nodiscard]] TweenSequenceStatus2D CancelActiveChildren(Slot& slot) noexcept;
    [[nodiscard]] TweenSequenceStatus2D CancelForChild(
        Slot& slot,
        runtime::TweenCancellationReason2D reason) noexcept;
    [[nodiscard]] TweenSequenceStatus2D WrapBinding(TweenBindingStatus2D status) const noexcept;
    void Complete(Slot& slot) noexcept;
    void IncrementActive() noexcept;
    void DecrementActive() noexcept;

    TweenBindingSystem2D& bindings_;
    std::vector<Slot> slots_{};
    std::size_t reservedChildCapacityPerSequence_{0U};
    std::uint64_t activeSequenceCount_{0U};
    std::uint64_t highWaterActiveSequenceCount_{0U};
    std::uint64_t createdSequenceCount_{0U};
    std::uint64_t reusedSequenceSlotCount_{0U};
};
} // namespace trace2d::scene
