#pragma once

#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <vector>

namespace trace2d::testing
{
class SpriteAnimatorReferenceModel final
{
public:
    explicit SpriteAnimatorReferenceModel(runtime::SpriteAnimator2DState state) noexcept;

    [[nodiscard]] const runtime::SpriteAnimator2DState& State() const noexcept;

    [[nodiscard]] std::vector<runtime::SpriteAnimationEmission2D> Advance(
        runtime::SpriteAnimationTime2D delta);

private:
    void AdvanceOneTimelineNanosecond(
        std::vector<runtime::SpriteAnimationEmission2D>& transcript);
    [[nodiscard]] bool PrepareTimelineStep(
        std::vector<runtime::SpriteAnimationEmission2D>& transcript);
    void HandleForwardEndpoint(
        std::vector<runtime::SpriteAnimationEmission2D>& transcript);
    void HandleReverseEndpoint(
        std::vector<runtime::SpriteAnimationEmission2D>& transcript);
    void EmitEventsAt(
        runtime::SpriteAnimationTime2D time,
        runtime::SpriteAnimationDirection direction,
        std::vector<runtime::SpriteAnimationEmission2D>& transcript) const;
    void EmitStructural(
        runtime::SpriteAnimationEmissionKind kind,
        runtime::SpriteAnimationTime2D time,
        runtime::SpriteAnimationDirection direction,
        std::vector<runtime::SpriteAnimationEmission2D>& transcript) const;
    void RefreshFrameIndex() noexcept;

    runtime::SpriteAnimator2DState state_{};
};
} // namespace trace2d::testing
