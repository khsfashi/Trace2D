#include <trace2d/scene/TweenBinding2D.hpp>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] bool IsValidTimeDomain(const runtime::TweenTimeDomain2D domain) noexcept
{
    return domain == runtime::TweenTimeDomain2D::Simulation ||
        domain == runtime::TweenTimeDomain2D::Presentation;
}
} // namespace

TweenBindingStatus2D TweenBindingSystem2D::Step(
    const runtime::TweenTimeDomain2D domain,
    const runtime::TweenTime2D delta) noexcept
{
    if (!IsValidTimeDomain(domain))
    {
        return Wrap({runtime::Tween2DError::InvalidTimeDomain});
    }
    if (delta.count() < 0)
    {
        return Wrap({runtime::Tween2DError::NegativeDelta});
    }

    for (BindingSlot& slot : bindings_)
    {
        if (!slot.occupied)
        {
            continue;
        }
        runtime::TweenState2D state{};
        if (!pool_.Inspect(slot.tween, state).Succeeded() ||
            state.playback != runtime::TweenPlaybackState2D::Playing || state.domain != domain)
        {
            continue;
        }

        if (!ValidateBinding(slot.binding).Succeeded())
        {
            if (pool_.Cancel(slot.tween, runtime::TweenCancellationReason2D::TargetInvalidated).Succeeded())
            {
                ++targetInvalidatedCount_;
            }
            continue;
        }

        if (slot.capturePending)
        {
            const runtime::TweenTime2D delay = slot.authoredSpec.tween.delay;
            const runtime::TweenTime2D remaining = delay - state.delayElapsed;
            if (state.delayElapsed >= delay || delta >= remaining)
            {
                const TweenBindingStatus2D captureStatus = PrepareCapture(slot, slot.tween);
                if (!captureStatus.Succeeded())
                {
                    if (pool_.Cancel(
                            slot.tween,
                            runtime::TweenCancellationReason2D::TargetInvalidated).Succeeded())
                    {
                        ++targetInvalidatedCount_;
                    }
                }
            }
        }
    }

    const runtime::Tween2DStatus stepStatus = pool_.Step(domain, delta);
    if (!stepStatus.Succeeded())
    {
        return Wrap(stepStatus);
    }

    for (BindingSlot& slot : bindings_)
    {
        if (!slot.occupied)
        {
            continue;
        }
        runtime::TweenState2D state{};
        if (!pool_.Inspect(slot.tween, state).Succeeded() || state.domain != domain ||
            state.playback == runtime::TweenPlaybackState2D::Cancelled ||
            state.playback == runtime::TweenPlaybackState2D::Paused)
        {
            continue;
        }
        if (state.delayElapsed < slot.authoredSpec.tween.delay || slot.capturePending)
        {
            continue;
        }
        if (!ValidateBinding(slot.binding).Succeeded())
        {
            if (pool_.Cancel(slot.tween, runtime::TweenCancellationReason2D::TargetInvalidated).Succeeded())
            {
                ++targetInvalidatedCount_;
            }
            continue;
        }
        if (slot.hasAppliedValue && slot.lastAppliedValue == state.currentValue)
        {
            continue;
        }
        if (!WriteBinding(slot.binding, state.currentValue))
        {
            (void)pool_.Cancel(
                slot.tween,
                runtime::TweenCancellationReason2D::PropertyWriteRejected);
            ++propertyWriteRejectedCount_;
            continue;
        }
        slot.lastAppliedValue = state.currentValue;
        slot.hasAppliedValue = true;
        ++appliedWriteCount_;
    }
    return {};
}

TweenBindingMetrics2D TweenBindingSystem2D::Metrics() const noexcept
{
    return TweenBindingMetrics2D{
        createdCount_,
        appliedWriteCount_,
        capturedStartCount_,
        conflictRejectedCount_,
        conflictReplacedCount_,
        targetInvalidatedCount_,
        propertyWriteRejectedCount_,
        static_cast<std::uint64_t>(bindings_.size()),
        static_cast<std::uint64_t>(bindings_.capacity()),
        static_cast<std::uint64_t>(externalProviders_.size()),
        static_cast<std::uint64_t>(externalProviders_.capacity()),
    };
}

runtime::TweenPoolMetrics2D TweenBindingSystem2D::PoolMetrics() const noexcept
{
    return pool_.Metrics();
}
} // namespace trace2d::scene
