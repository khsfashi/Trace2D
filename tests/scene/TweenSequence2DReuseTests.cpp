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
    const runtime::TweenTime2D duration)
{
    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = runtime::TweenTimeDomain2D::Presentation;
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

TEST(TweenSequence2DReuseTests, JoinAfterIntervalUsesIntervalGroupStart)
{
    scene::Scene scene{};
    const scene::EntityId lead = scene.CreateEntity(Entity("lead"));
    const scene::EntityId joined = scene.CreateEntity(Entity("joined"));
    scene::TweenBindingSystem2D bindings{scene};

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Append(
        ResolveRotation(bindings, lead),
        FloatSpec(0.0F, 10.0F, 10ns)).Succeeded());
    ASSERT_TRUE(definition.Interval(5ns).Succeeded());
    ASSERT_TRUE(definition.Join(
        ResolveRotation(bindings, joined),
        FloatSpec(0.0F, 5.0F, 5ns)).Succeeded());
    EXPECT_EQ(definition.Duration(), 15ns);

    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 10ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(lead)->LocalTransform().rotationRadians, 10.0F);
    EXPECT_FLOAT_EQ(scene.TryGet(joined)->LocalTransform().rotationRadians, 0.0F);

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 2ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(joined)->LocalTransform().rotationRadians, 2.0F);
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 3ns).Succeeded());

    scene::TweenSequenceState2D state{};
    ASSERT_TRUE(sequences.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, scene::TweenSequencePlaybackState2D::Completed);
    EXPECT_EQ(state.completedChildCount, 2U);
}

TEST(TweenSequence2DReuseTests, SequentialChildrenRetireCompletedHandlesBeforePoolReuse)
{
    scene::Scene scene{};
    const scene::EntityId entity = scene.CreateEntity(Entity("reuse"));
    scene::TweenBindingSystem2D bindings{scene};
    const scene::ResolvedTweenBinding2D rotation = ResolveRotation(bindings, entity);

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Append(rotation, FloatSpec(0.0F, 10.0F, 10ns)).Succeeded());
    ASSERT_TRUE(definition.Append(rotation, FloatSpec(10.0F, 20.0F, 10ns)).Succeeded());

    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 20ns).Succeeded());

    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 20.0F);
    EXPECT_GE(bindings.PoolMetrics().reusedSlotCount, 1U);

    scene::TweenSequenceState2D state{};
    ASSERT_TRUE(sequences.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, scene::TweenSequencePlaybackState2D::Completed);
    EXPECT_EQ(state.completedChildCount, 2U);
    EXPECT_EQ(state.activeChildCount, 0U);
}
} // namespace
