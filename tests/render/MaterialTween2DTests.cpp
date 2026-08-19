#include <trace2d/render/MaterialTween2D.hpp>
#include <trace2d/scene/TweenSequence2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d;

struct PreparedMaterial final
{
    render::MaterialParameterLayout2D layout{};
    render::MaterialParameterBlock2D block{};
    render::MaterialParameterBinding2D amount{};
    render::MaterialParameterBinding2D offset{};
    render::MaterialParameterBinding2D tint{};
};

[[nodiscard]] bool PrepareMaterial(PreparedMaterial& out) noexcept
{
    const assets::ResourceHandleUntyped shader{
        7U,
        1U,
        assets::ResourceTypeDomain::Shader2D,
    };
    const std::array<render::MaterialParameterDeclaration2D, 3U> declarations{{
        {"amount", render::MaterialParameterType2D::Float},
        {"offset", render::MaterialParameterType2D::Float2},
        {"tint", render::MaterialParameterType2D::Color},
    }};
    if (!render::PrepareMaterialParameterLayout2D(shader, declarations, out.layout).Succeeded())
    {
        return false;
    }
    const std::array<render::MaterialParameterValue2D, 3U> defaults{{
        render::MaterialFloat2D(0.25F),
        render::MaterialFloat2D(1.0F, 2.0F),
        render::MaterialColor2D(0.1F, 0.2F, 0.3F, 1.0F),
    }};
    if (!render::PrepareMaterialParameterBlock2D(out.layout, defaults, out.block).Succeeded())
    {
        return false;
    }
    return render::ResolveMaterialParameterBinding2D(out.layout, "amount", out.amount).Succeeded() &&
        render::ResolveMaterialParameterBinding2D(out.layout, "offset", out.offset).Succeeded() &&
        render::ResolveMaterialParameterBinding2D(out.layout, "tint", out.tint).Succeeded();
}

[[nodiscard]] scene::TweenBindingSpec2D RelativeSpec(
    const runtime::TweenValue2D& delta,
    const runtime::TweenTime2D duration = 10ns) noexcept
{
    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = runtime::TweenTimeDomain2D::Presentation;
    spec.tween.duration = duration;
    spec.tween.end = delta;
    spec.startMode = scene::TweenStartMode2D::CaptureCurrent;
    spec.endMode = scene::TweenEndMode2D::Relative;
    return spec;
}

[[nodiscard]] float Lane(
    const render::MaterialParameterBlock2D& block,
    const render::MaterialParameterBinding2D binding,
    const std::size_t lane = 0U) noexcept
{
    return block.packed[
        static_cast<std::size_t>(binding.slot) * render::Material2DParameterSlotFloatCount + lane];
}

TEST(MaterialTween2DTests, FloatFloat2AndColorBindingsReuseT2AuthorityWithoutNameLookupAtStep)
{
    PreparedMaterial material{};
    ASSERT_TRUE(PrepareMaterial(material));
    const std::uint64_t initialValueIdentity = material.block.valueIdentity;

    scene::Scene scene{};
    scene::TweenBindingSystem2D tweens{scene};
    tweens.Reserve(3U);
    tweens.ReserveExternalProviders(1U);

    render::MaterialTweenTargetPool2D targets{};
    targets.Reserve(1U);
    render::MaterialTweenTargetHandle2D target{};
    ASSERT_TRUE(targets.Create(material.block, target).Succeeded());

    scene::TweenExternalProviderHandle2D provider{};
    ASSERT_TRUE(tweens.RegisterExternalProvider(targets.ExternalProvider(), provider).Succeeded());

    scene::ResolvedTweenBinding2D amount{};
    scene::ResolvedTweenBinding2D offset{};
    scene::ResolvedTweenBinding2D tint{};
    ASSERT_TRUE(targets.ResolveBinding(tweens, provider, target, material.amount, amount).Succeeded());
    ASSERT_TRUE(targets.ResolveBinding(tweens, provider, target, material.offset, offset).Succeeded());
    ASSERT_TRUE(targets.ResolveBinding(tweens, provider, target, material.tint, tint).Succeeded());
    EXPECT_EQ(amount.valueType, runtime::TweenValueType2D::Float);
    EXPECT_EQ(offset.valueType, runtime::TweenValueType2D::Float2);
    EXPECT_EQ(tint.valueType, runtime::TweenValueType2D::Color);

    runtime::TweenHandle2D amountTween{};
    runtime::TweenHandle2D offsetTween{};
    runtime::TweenHandle2D tintTween{};
    ASSERT_TRUE(tweens.Create(
        amount,
        RelativeSpec(runtime::TweenValue2D::Float(0.5F)),
        amountTween).Succeeded());
    ASSERT_TRUE(tweens.Create(
        offset,
        RelativeSpec(runtime::TweenValue2D::Float2(2.0F, 4.0F)),
        offsetTween).Succeeded());
    ASSERT_TRUE(tweens.Create(
        tint,
        RelativeSpec(runtime::TweenValue2D::Color(0.2F, 0.2F, 0.2F, -0.5F)),
        tintTween).Succeeded());

    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    const render::MaterialParameterBlock2D* const block = targets.Resolve(target);
    ASSERT_NE(block, nullptr);
    EXPECT_FLOAT_EQ(Lane(*block, material.amount), 0.5F);
    EXPECT_FLOAT_EQ(Lane(*block, material.offset, 0U), 2.0F);
    EXPECT_FLOAT_EQ(Lane(*block, material.offset, 1U), 4.0F);
    EXPECT_FLOAT_EQ(Lane(*block, material.tint, 0U), 0.2F);
    EXPECT_FLOAT_EQ(Lane(*block, material.tint, 1U), 0.3F);
    EXPECT_FLOAT_EQ(Lane(*block, material.tint, 2U), 0.4F);
    EXPECT_FLOAT_EQ(Lane(*block, material.tint, 3U), 0.75F);
    EXPECT_NE(block->valueIdentity, initialValueIdentity);

    const scene::TweenBindingMetrics2D bindingMetrics = tweens.Metrics();
    EXPECT_EQ(bindingMetrics.retainedExternalProviderCount, 1U);
    EXPECT_GE(bindingMetrics.retainedExternalProviderCapacity, 1U);
    EXPECT_GE(bindingMetrics.retainedBindingCapacity, 3U);
    EXPECT_GE(targets.Metrics().retainedTargetCapacity, 1U);
    EXPECT_GE(targets.Metrics().appliedWriteCount, 6U);
}

TEST(MaterialTween2DTests, MaterialConflictRejectReplaceUsesExistingT2WriterAuthority)
{
    PreparedMaterial material{};
    ASSERT_TRUE(PrepareMaterial(material));

    scene::Scene scene{};
    scene::TweenBindingSystem2D tweens{scene};
    render::MaterialTweenTargetPool2D targets{};
    render::MaterialTweenTargetHandle2D target{};
    ASSERT_TRUE(targets.Create(material.block, target).Succeeded());
    scene::TweenExternalProviderHandle2D provider{};
    ASSERT_TRUE(tweens.RegisterExternalProvider(targets.ExternalProvider(), provider).Succeeded());

    scene::ResolvedTweenBinding2D binding{};
    ASSERT_TRUE(targets.ResolveBinding(
        tweens, provider, target, material.amount, binding).Succeeded());

    runtime::TweenHandle2D first{};
    ASSERT_TRUE(tweens.Create(
        binding,
        RelativeSpec(runtime::TweenValue2D::Float(1.0F), 100ns),
        first).Succeeded());

    runtime::TweenHandle2D rejected{};
    EXPECT_EQ(
        tweens.Create(
            binding,
            RelativeSpec(runtime::TweenValue2D::Float(2.0F), 100ns),
            rejected).error,
        scene::TweenBindingError2D::ConflictRejected);

    scene::TweenBindingSpec2D replacement =
        RelativeSpec(runtime::TweenValue2D::Float(3.0F), 100ns);
    replacement.conflictPolicy = scene::TweenConflictPolicy2D::Replace;
    runtime::TweenHandle2D replacementHandle{};
    ASSERT_TRUE(tweens.Create(binding, replacement, replacementHandle).Succeeded());

    runtime::TweenState2D firstState{};
    ASSERT_TRUE(tweens.Inspect(first, firstState).Succeeded());
    EXPECT_EQ(firstState.playback, runtime::TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(firstState.cancellationReason, runtime::TweenCancellationReason2D::Replaced);
    EXPECT_EQ(tweens.Metrics().conflictRejectedCount, 1U);
    EXPECT_EQ(tweens.Metrics().conflictReplacedCount, 1U);
}

TEST(MaterialTween2DTests, DestroyedAndRecycledMaterialTargetInvalidatesStaleTween)
{
    PreparedMaterial material{};
    ASSERT_TRUE(PrepareMaterial(material));

    scene::Scene scene{};
    scene::TweenBindingSystem2D tweens{scene};
    render::MaterialTweenTargetPool2D targets{};
    targets.Reserve(1U);

    render::MaterialTweenTargetHandle2D oldTarget{};
    ASSERT_TRUE(targets.Create(material.block, oldTarget).Succeeded());
    scene::TweenExternalProviderHandle2D provider{};
    ASSERT_TRUE(tweens.RegisterExternalProvider(targets.ExternalProvider(), provider).Succeeded());
    scene::ResolvedTweenBinding2D oldBinding{};
    ASSERT_TRUE(targets.ResolveBinding(
        tweens, provider, oldTarget, material.amount, oldBinding).Succeeded());

    runtime::TweenHandle2D tween{};
    ASSERT_TRUE(tweens.Create(
        oldBinding,
        RelativeSpec(runtime::TweenValue2D::Float(1.0F), 100ns),
        tween).Succeeded());

    ASSERT_TRUE(targets.Destroy(oldTarget).Succeeded());
    render::MaterialTweenTargetHandle2D replacement{};
    ASSERT_TRUE(targets.Create(material.block, replacement).Succeeded());
    EXPECT_EQ(replacement.index, oldTarget.index);
    EXPECT_NE(replacement.generation, oldTarget.generation);

    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    runtime::TweenState2D state{};
    ASSERT_TRUE(tweens.Inspect(tween, state).Succeeded());
    EXPECT_EQ(state.playback, runtime::TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(state.cancellationReason, runtime::TweenCancellationReason2D::TargetInvalidated);
    EXPECT_EQ(tweens.Metrics().targetInvalidatedCount, 1U);

    const render::MaterialParameterBlock2D* const replacementBlock = targets.Resolve(replacement);
    ASSERT_NE(replacementBlock, nullptr);
    EXPECT_FLOAT_EQ(Lane(*replacementBlock, material.amount), 0.25F);
}

TEST(MaterialTween2DTests, SequenceAnimatesMaterialAndCompletionRemainsInspectableStateOnly)
{
    PreparedMaterial material{};
    ASSERT_TRUE(PrepareMaterial(material));

    scene::Scene scene{};
    scene::TweenBindingSystem2D tweens{scene};
    render::MaterialTweenTargetPool2D targets{};
    render::MaterialTweenTargetHandle2D target{};
    ASSERT_TRUE(targets.Create(material.block, target).Succeeded());
    scene::TweenExternalProviderHandle2D provider{};
    ASSERT_TRUE(tweens.RegisterExternalProvider(targets.ExternalProvider(), provider).Succeeded());

    scene::ResolvedTweenBinding2D amount{};
    scene::ResolvedTweenBinding2D tint{};
    ASSERT_TRUE(targets.ResolveBinding(tweens, provider, target, material.amount, amount).Succeeded());
    ASSERT_TRUE(targets.ResolveBinding(tweens, provider, target, material.tint, tint).Succeeded());

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Append(
        amount,
        RelativeSpec(runtime::TweenValue2D::Float(0.5F), 10ns)).Succeeded());
    ASSERT_TRUE(definition.Join(
        tint,
        RelativeSpec(runtime::TweenValue2D::Color(0.2F, 0.2F, 0.2F, -0.5F), 10ns)).Succeeded());

    scene::TweenSequenceSystem2D sequences{tweens};
    sequences.Reserve(1U, 2U);
    scene::TweenSequenceHandle2D sequence{};
    ASSERT_TRUE(sequences.Create(definition, sequence).Succeeded());
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 10ns).Succeeded());

    scene::TweenSequenceState2D state{};
    ASSERT_TRUE(sequences.Inspect(sequence, state).Succeeded());
    EXPECT_EQ(state.playback, scene::TweenSequencePlaybackState2D::Completed);
    EXPECT_EQ(state.cancellationReason, scene::TweenSequenceCancellationReason2D::None);
    EXPECT_EQ(state.completedChildCount, 2U);

    const render::MaterialParameterBlock2D* const block = targets.Resolve(target);
    ASSERT_NE(block, nullptr);
    EXPECT_FLOAT_EQ(Lane(*block, material.amount), 0.75F);
    EXPECT_FLOAT_EQ(Lane(*block, material.tint, 0U), 0.3F);
    EXPECT_FLOAT_EQ(Lane(*block, material.tint, 3U), 0.5F);
}
} // namespace
