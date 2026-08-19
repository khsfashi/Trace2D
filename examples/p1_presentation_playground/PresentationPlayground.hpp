#pragma once

#include <trace2d/render/MaterialTween2D.hpp>
#include <trace2d/scene/TweenSequence2D.hpp>

#include <chrono>

namespace trace2d::examples
{
struct PresentationProofMetrics final
{
    scene::TweenBindingMetrics2D bindings{};
    scene::TweenSequenceMetrics2D sequences{};
    render::MaterialTweenTargetMetrics2D materialTargets{};
};

class PresentationPlayground final
{
public:
    PresentationPlayground(
        const render::MaterialParameterLayout2D& materialLayout,
        const render::MaterialParameterBlock2D& materialDefaults);

    [[nodiscard]] bool StartShowcase() noexcept;
    [[nodiscard]] bool RestartShowcase() noexcept;
    [[nodiscard]] bool Step(std::chrono::nanoseconds delta) noexcept;

    [[nodiscard]] bool AllCompleted() const noexcept;
    [[nodiscard]] float FlashAmount() const noexcept;
    [[nodiscard]] const scene::Transform2D* ButtonTransform() const noexcept;
    [[nodiscard]] const scene::Transform2D* PanelTransform() const noexcept;
    [[nodiscard]] const render::MaterialParameterBlock2D* MaterialParameters() const noexcept;
    [[nodiscard]] PresentationProofMetrics Metrics() const noexcept;

private:
    [[nodiscard]] static scene::TweenBindingSpec2D Absolute(
        runtime::TweenValue2D start,
        runtime::TweenValue2D end,
        std::chrono::nanoseconds duration,
        runtime::TweenEasing2D easing) noexcept;

    scene::Scene scene_{};
    scene::TweenBindingSystem2D tweens_;
    scene::TweenSequenceSystem2D sequences_;
    render::MaterialTweenTargetPool2D materialTargets_{};

    scene::EntityId button_{};
    scene::EntityId panel_{};
    render::MaterialTweenTargetHandle2D materialTarget_{};
    render::MaterialParameterBinding2D flashAmountParameter_{};
    scene::TweenExternalProviderHandle2D materialProvider_{};
    scene::ResolvedTweenBinding2D flashAmountBinding_{};
    scene::ResolvedTweenBinding2D buttonScaleBinding_{};
    scene::ResolvedTweenBinding2D panelPositionBinding_{};

    scene::TweenSequenceHandle2D flashSequence_{};
    scene::TweenSequenceHandle2D buttonSequence_{};
    scene::TweenSequenceHandle2D panelSequence_{};
    bool started_{false};
};
} // namespace trace2d::examples
