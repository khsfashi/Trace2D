#include <trace2d/scene/TweenSequence2D.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <utility>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d;

[[nodiscard]] scene::EntityDescriptor Entity(std::string semanticId)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    return descriptor;
}

[[nodiscard]] scene::TweenBindingSpec2D FloatSpec(
    const float start,
    const float end,
    const runtime::TweenTime2D duration = 10ns,
    const runtime::TweenTimeDomain2D domain = runtime::TweenTimeDomain2D::Presentation)
{
    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = domain;
    spec.tween.duration = duration;
    spec.tween.start = runtime::TweenValue2D::Float(start);
    spec.tween.end = runtime::TweenValue2D::Float(end);
    return spec;
}

[[nodiscard]] scene::ResolvedTweenBinding2D ResolveRotation(
    scene::TweenBindingSystem2D& bindings,
    const scene::EntityId entity)
{
    scene::ResolvedTweenBinding2D binding{};
    EXPECT_TRUE(bindings.ResolveTransform(
        entity,
        scene::TransformTweenProperty2D::RotationRadians,
        binding).Succeeded());
    return binding;
}

TEST(TweenSequence2DTests, ExternalWriterConflictUsesExistingRejectAndReplaceAuthority)
{
    scene::Scene scene{};
    const scene::EntityId entity = scene.CreateEntity(Entity("external-conflict"));
    scene::TweenBindingSystem2D bindings{scene};
    const scene::ResolvedTweenBinding2D rotation = ResolveRotation(bindings, entity);

    runtime::TweenHandle2D external{};
    ASSERT_TRUE(bindings.Create(rotation, FloatSpec(0.0F, 100.0F, 100ns), external).Succeeded());

    scene::TweenSequenceDefinition2D rejectedDefinition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(rejectedDefinition.Append(rotation, FloatSpec(1.0F, 2.0F)).Succeeded());
    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D rejected{};
    const scene::TweenSequenceStatus2D rejectedStatus = sequences.Create(rejectedDefinition, rejected);
    EXPECT_EQ(rejectedStatus.error, scene::TweenSequenceError2D::BindingFailure);
    EXPECT_EQ(rejectedStatus.bindingError, scene::TweenBindingError2D::ConflictRejected);

    runtime::TweenState2D externalState{};
    ASSERT_TRUE(bindings.Inspect(external, externalState).Succeeded());
    EXPECT_EQ(externalState.playback, runtime::TweenPlaybackState2D::Playing);

    auto replacementSpec = FloatSpec(5.0F, 15.0F);
    replacementSpec.conflictPolicy = scene::TweenConflictPolicy2D::Replace;
    scene::TweenSequenceDefinition2D replacementDefinition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(replacementDefinition.Append(rotation, replacementSpec).Succeeded());
    scene::TweenSequenceHandle2D replacement{};
    ASSERT_TRUE(sequences.Create(replacementDefinition, replacement).Succeeded());

    ASSERT_TRUE(bindings.Inspect(external, externalState).Succeeded());
    EXPECT_EQ(externalState.playback, runtime::TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(externalState.cancellationReason, runtime::TweenCancellationReason2D::Replaced);
}

TEST(TweenSequence2DTests, CaptureCurrentSamplesAtScheduledActivationAndInvalidationCancelsSequence)
{
    scene::Scene scene{};
    auto descriptor = Entity("capture");
    descriptor.transform.rotationRadians = 1.0F;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));
    scene::TweenBindingSystem2D bindings{scene};
    const scene::ResolvedTweenBinding2D rotation = ResolveRotation(bindings, entity);

    scene::TweenBindingSpec2D capture = FloatSpec(0.0F, 10.0F);
    capture.startMode = scene::TweenStartMode2D::CaptureCurrent;
    capture.endMode = scene::TweenEndMode2D::Relative;

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Interval(10ns).Succeeded());
    ASSERT_TRUE(definition.Append(rotation, capture).Succeeded());
    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());

    scene.TryGet(entity)->LocalTransform().rotationRadians = 7.0F;
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 10ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 7.0F);
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 12.0F);

    ASSERT_TRUE(scene.DestroyEntity(entity));
    auto replacementDescriptor = Entity("replacement");
    replacementDescriptor.transform.rotationRadians = 99.0F;
    const scene::EntityId replacement = scene.CreateEntity(std::move(replacementDescriptor));
    EXPECT_EQ(replacement.index, entity.index);
    EXPECT_NE(replacement.generation, entity.generation);

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    scene::TweenSequenceState2D state{};
    ASSERT_TRUE(sequences.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, scene::TweenSequencePlaybackState2D::Cancelled);
    EXPECT_EQ(
        state.cancellationReason,
        scene::TweenSequenceCancellationReason2D::TargetInvalidated);
    EXPECT_FLOAT_EQ(scene.TryGet(replacement)->LocalTransform().rotationRadians, 99.0F);
}

TEST(TweenSequence2DTests, PauseResumeRestartAndYoyoReuseRetainedSlots)
{
    scene::Scene scene{};
    const scene::EntityId entity = scene.CreateEntity(Entity("lifecycle"));
    scene::TweenBindingSystem2D bindings{scene};
    const scene::ResolvedTweenBinding2D rotation = ResolveRotation(bindings, entity);

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Append(rotation, FloatSpec(0.0F, 10.0F)).Succeeded());

    scene::TweenSequenceSystem2D sequences{bindings};
    sequences.Reserve(1U, 4U);
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 4ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 4.0F);

    ASSERT_TRUE(sequences.Pause(handle).Succeeded());
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 6ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 4.0F);
    ASSERT_TRUE(sequences.Resume(handle).Succeeded());
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 6ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 10.0F);

    ASSERT_TRUE(sequences.Restart(handle).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 0.0F);
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 5.0F);
    ASSERT_TRUE(sequences.Cancel(handle).Succeeded());

    auto yoyoSpec = FloatSpec(5.0F, 15.0F, 5ns);
    yoyoSpec.tween.repeatCount = 1U;
    yoyoSpec.tween.loopMode = runtime::TweenLoopMode2D::Yoyo;
    scene::TweenSequenceDefinition2D yoyo{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(yoyo.Append(rotation, yoyoSpec).Succeeded());
    EXPECT_EQ(yoyo.Duration(), 10ns);

    scene::TweenSequenceHandle2D yoyoHandle{};
    ASSERT_TRUE(sequences.Create(yoyo, yoyoHandle).Succeeded());
    EXPECT_EQ(yoyoHandle.index, handle.index);
    EXPECT_NE(yoyoHandle.generation, handle.generation);
    scene::TweenSequenceState2D staleState{};
    EXPECT_EQ(
        sequences.Inspect(handle, staleState).error,
        scene::TweenSequenceError2D::InvalidHandle);

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 10ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 5.0F);

    scene::TweenSequenceState2D yoyoState{};
    ASSERT_TRUE(sequences.Inspect(yoyoHandle, yoyoState).Succeeded());
    EXPECT_EQ(yoyoState.playback, scene::TweenSequencePlaybackState2D::Completed);

    const scene::TweenSequenceMetrics2D metrics = sequences.Metrics();
    EXPECT_EQ(metrics.retainedSequenceSlotCount, 1U);
    EXPECT_GE(metrics.retainedChildCapacity, 4U);
    EXPECT_GE(metrics.reusedSequenceSlotCount, 1U);
}
} // namespace
