#include "PresentationPlayground.hpp"

#include <stdexcept>

namespace trace2d::examples
{
namespace
{
using namespace std::chrono_literals;

[[nodiscard]] bool Completed(
    const scene::TweenSequenceSystem2D& sequences,
    const scene::TweenSequenceHandle2D handle) noexcept
{
    scene::TweenSequenceState2D state{};
    return sequences.Inspect(handle, state).Succeeded() &&
        state.playback == scene::TweenSequencePlaybackState2D::Completed;
}
} // namespace

PresentationPlayground::PresentationPlayground(
    const render::MaterialParameterLayout2D& materialLayout,
    const render::MaterialParameterBlock2D& materialDefaults)
    : tweens_{scene_},
      sequences_{tweens_}
{
    tweens_.Reserve(8U);
    tweens_.ReserveExternalProviders(1U);
    sequences_.Reserve(4U, 2U);
    materialTargets_.Reserve(1U);

    button_ = scene_.CreateEntity(scene::EntityDescriptor{
        .semanticId = "presentation.button",
        .name = "Button punch target",
        .transform = scene::Transform2D{
            .position = {-170.0F, 90.0F},
            .rotationRadians = 0.0F,
            .scale = {1.0F, 1.0F},
        },
    });
    panel_ = scene_.CreateEntity(scene::EntityDescriptor{
        .semanticId = "presentation.panel",
        .name = "Panel slide target",
        .transform = scene::Transform2D{
            .position = {-260.0F, -100.0F},
            .rotationRadians = 0.0F,
            .scale = {1.0F, 1.0F},
        },
    });

    if (!render::ResolveMaterialParameterBinding2D(
            materialLayout,
            "flashAmount",
            flashAmountParameter_).Succeeded())
    {
        throw std::runtime_error{"P1 could not resolve the hit_flash material parameter."};
    }
    if (!materialTargets_.Create(materialDefaults, materialTarget_).Succeeded())
    {
        throw std::runtime_error{"P1 could not retain the Material2D presentation target."};
    }
    if (!tweens_.RegisterExternalProvider(
            materialTargets_.ExternalProvider(),
            materialProvider_).Succeeded())
    {
        throw std::runtime_error{"P1 could not register the Material2D tween provider."};
    }
    if (!materialTargets_.ResolveBinding(
            tweens_,
            materialProvider_,
            materialTarget_,
            flashAmountParameter_,
            flashAmountBinding_).Succeeded())
    {
        throw std::runtime_error{"P1 could not resolve the hit_flash Tween2D binding."};
    }
    if (!tweens_.ResolveTransform(
            button_,
            scene::TransformTweenProperty2D::Scale,
            buttonScaleBinding_).Succeeded())
    {
        throw std::runtime_error{"P1 could not resolve the button_punch scale binding."};
    }
    if (!tweens_.ResolveTransform(
            panel_,
            scene::TransformTweenProperty2D::Position,
            panelPositionBinding_).Succeeded())
    {
        throw std::runtime_error{"P1 could not resolve the panel_slide position binding."};
    }
}

scene::TweenBindingSpec2D PresentationPlayground::Absolute(
    const runtime::TweenValue2D start,
    const runtime::TweenValue2D end,
    const std::chrono::nanoseconds duration,
    const runtime::TweenEasing2D easing) noexcept
{
    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = runtime::TweenTimeDomain2D::Presentation;
    spec.tween.duration = duration;
    spec.tween.start = start;
    spec.tween.end = end;
    spec.tween.easing = easing;
    spec.startMode = scene::TweenStartMode2D::Explicit;
    spec.endMode = scene::TweenEndMode2D::Absolute;
    spec.conflictPolicy = scene::TweenConflictPolicy2D::Reject;
    return spec;
}

bool PresentationPlayground::StartShowcase() noexcept
{
    if (started_)
    {
        return RestartShowcase();
    }

    scene::TweenSequenceDefinition2D flash{runtime::TweenTimeDomain2D::Presentation};
    if (!flash.Append(
            flashAmountBinding_,
            Absolute(
                runtime::TweenValue2D::Float(0.0F),
                runtime::TweenValue2D::Float(1.0F),
                80ms,
                runtime::TweenEasing2D::EaseOutQuad)).Succeeded() ||
        !flash.Append(
            flashAmountBinding_,
            Absolute(
                runtime::TweenValue2D::Float(1.0F),
                runtime::TweenValue2D::Float(0.0F),
                80ms,
                runtime::TweenEasing2D::EaseInQuad)).Succeeded())
    {
        return false;
    }

    scene::TweenSequenceDefinition2D button{runtime::TweenTimeDomain2D::Presentation};
    if (!button.Append(
            buttonScaleBinding_,
            Absolute(
                runtime::TweenValue2D::Float2(1.0F, 1.0F),
                runtime::TweenValue2D::Float2(1.18F, 1.18F),
                90ms,
                runtime::TweenEasing2D::EaseOutCubic)).Succeeded() ||
        !button.Append(
            buttonScaleBinding_,
            Absolute(
                runtime::TweenValue2D::Float2(1.18F, 1.18F),
                runtime::TweenValue2D::Float2(1.0F, 1.0F),
                90ms,
                runtime::TweenEasing2D::EaseInOutQuad)).Succeeded())
    {
        return false;
    }

    scene::TweenSequenceDefinition2D panel{runtime::TweenTimeDomain2D::Presentation};
    if (!panel.Append(
            panelPositionBinding_,
            Absolute(
                runtime::TweenValue2D::Float2(-260.0F, -100.0F),
                runtime::TweenValue2D::Float2(160.0F, -100.0F),
                260ms,
                runtime::TweenEasing2D::EaseOutCubic)).Succeeded())
    {
        return false;
    }

    if (!sequences_.Create(flash, flashSequence_).Succeeded() ||
        !sequences_.Create(button, buttonSequence_).Succeeded() ||
        !sequences_.Create(panel, panelSequence_).Succeeded())
    {
        return false;
    }
    started_ = true;
    return true;
}

bool PresentationPlayground::RestartShowcase() noexcept
{
    if (!started_)
    {
        return StartShowcase();
    }
    return sequences_.Restart(flashSequence_).Succeeded() &&
        sequences_.Restart(buttonSequence_).Succeeded() &&
        sequences_.Restart(panelSequence_).Succeeded();
}

bool PresentationPlayground::Step(const std::chrono::nanoseconds delta) noexcept
{
    return started_ &&
        sequences_.Step(runtime::TweenTimeDomain2D::Presentation, delta).Succeeded();
}

bool PresentationPlayground::AllCompleted() const noexcept
{
    return started_ &&
        Completed(sequences_, flashSequence_) &&
        Completed(sequences_, buttonSequence_) &&
        Completed(sequences_, panelSequence_);
}

float PresentationPlayground::FlashAmount() const noexcept
{
    const render::MaterialParameterBlock2D* const block = MaterialParameters();
    if (block == nullptr || flashAmountParameter_.slot >= block->parameterCount)
    {
        return 0.0F;
    }
    const std::size_t index =
        static_cast<std::size_t>(flashAmountParameter_.slot) *
        render::Material2DParameterSlotFloatCount;
    return block->packed[index];
}

const scene::Transform2D* PresentationPlayground::ButtonTransform() const noexcept
{
    const scene::Entity* const entity = scene_.TryGet(button_);
    return entity == nullptr ? nullptr : &entity->LocalTransform();
}

const scene::Transform2D* PresentationPlayground::PanelTransform() const noexcept
{
    const scene::Entity* const entity = scene_.TryGet(panel_);
    return entity == nullptr ? nullptr : &entity->LocalTransform();
}

const render::MaterialParameterBlock2D* PresentationPlayground::MaterialParameters() const noexcept
{
    return materialTargets_.Resolve(materialTarget_);
}

PresentationProofMetrics PresentationPlayground::Metrics() const noexcept
{
    return PresentationProofMetrics{
        tweens_.Metrics(),
        sequences_.Metrics(),
        materialTargets_.Metrics(),
    };
}
} // namespace trace2d::examples
