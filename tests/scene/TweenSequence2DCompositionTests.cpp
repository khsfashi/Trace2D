#include <trace2d/scene/TweenSequence2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
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

struct OrderedProbe final
{
    float first{0.0F};
    float second{0.0F};
    std::array<int, 16U> order{};
    std::size_t orderCount{0U};
};

[[nodiscard]] runtime::TweenValue2D ReadFirst(const OrderedProbe& probe) noexcept
{
    return runtime::TweenValue2D::Float(probe.first);
}

[[nodiscard]] runtime::TweenValue2D ReadSecond(const OrderedProbe& probe) noexcept
{
    return runtime::TweenValue2D::Float(probe.second);
}

[[nodiscard]] bool WriteFirst(OrderedProbe& probe, const runtime::TweenValue2D& value) noexcept
{
    if (value.type != runtime::TweenValueType2D::Float)
    {
        return false;
    }
    probe.first = value.components[0];
    if (probe.orderCount < probe.order.size())
    {
        probe.order[probe.orderCount++] = 1;
    }
    return true;
}

[[nodiscard]] bool WriteSecond(OrderedProbe& probe, const runtime::TweenValue2D& value) noexcept
{
    if (value.type != runtime::TweenValueType2D::Float)
    {
        return false;
    }
    probe.second = value.components[0];
    if (probe.orderCount < probe.order.size())
    {
        probe.order[probe.orderCount++] = 2;
    }
    return true;
}

TEST(TweenSequence2DTests, AppendJoinInsertAndIntervalHaveExactBoundariesAndLargeStepEquivalence)
{
    scene::Scene scene{};
    const scene::EntityId first = scene.CreateEntity(Entity("first"));
    const scene::EntityId second = scene.CreateEntity(Entity("second"));
    const scene::EntityId third = scene.CreateEntity(Entity("third"));
    const scene::EntityId inserted = scene.CreateEntity(Entity("inserted"));
    scene::TweenBindingSystem2D bindings{scene};

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Append(ResolveRotation(bindings, first), FloatSpec(0.0F, 10.0F)).Succeeded());
    ASSERT_TRUE(definition.Join(ResolveRotation(bindings, second), FloatSpec(0.0F, 20.0F, 20ns)).Succeeded());
    ASSERT_TRUE(definition.Interval(5ns).Succeeded());
    ASSERT_TRUE(definition.Append(ResolveRotation(bindings, third), FloatSpec(0.0F, 10.0F)).Succeeded());
    ASSERT_TRUE(definition.Insert(5ns, ResolveRotation(bindings, inserted), FloatSpec(0.0F, 5.0F, 5ns)).Succeeded());
    EXPECT_EQ(definition.Duration(), 35ns);

    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(first)->LocalTransform().rotationRadians, 5.0F);
    EXPECT_FLOAT_EQ(scene.TryGet(second)->LocalTransform().rotationRadians, 5.0F);
    EXPECT_FLOAT_EQ(scene.TryGet(inserted)->LocalTransform().rotationRadians, 0.0F);

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(first)->LocalTransform().rotationRadians, 10.0F);
    EXPECT_FLOAT_EQ(scene.TryGet(second)->LocalTransform().rotationRadians, 10.0F);
    EXPECT_FLOAT_EQ(scene.TryGet(inserted)->LocalTransform().rotationRadians, 5.0F);

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 15ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(second)->LocalTransform().rotationRadians, 20.0F);
    EXPECT_FLOAT_EQ(scene.TryGet(third)->LocalTransform().rotationRadians, 0.0F);
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 10ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(third)->LocalTransform().rotationRadians, 10.0F);

    scene::TweenSequenceState2D state{};
    ASSERT_TRUE(sequences.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, scene::TweenSequencePlaybackState2D::Completed);
    EXPECT_EQ(state.elapsed, 35ns);
    EXPECT_EQ(state.completedChildCount, 4U);

    scene::Scene largeScene{};
    const scene::EntityId largeFirst = largeScene.CreateEntity(Entity("first"));
    const scene::EntityId largeSecond = largeScene.CreateEntity(Entity("second"));
    const scene::EntityId largeThird = largeScene.CreateEntity(Entity("third"));
    const scene::EntityId largeInserted = largeScene.CreateEntity(Entity("inserted"));
    scene::TweenBindingSystem2D largeBindings{largeScene};
    scene::TweenSequenceDefinition2D largeDefinition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(largeDefinition.Append(ResolveRotation(largeBindings, largeFirst), FloatSpec(0.0F, 10.0F)).Succeeded());
    ASSERT_TRUE(largeDefinition.Join(ResolveRotation(largeBindings, largeSecond), FloatSpec(0.0F, 20.0F, 20ns)).Succeeded());
    ASSERT_TRUE(largeDefinition.Interval(5ns).Succeeded());
    ASSERT_TRUE(largeDefinition.Append(ResolveRotation(largeBindings, largeThird), FloatSpec(0.0F, 10.0F)).Succeeded());
    ASSERT_TRUE(largeDefinition.Insert(5ns, ResolveRotation(largeBindings, largeInserted), FloatSpec(0.0F, 5.0F, 5ns)).Succeeded());

    scene::TweenSequenceSystem2D largeSequences{largeBindings};
    scene::TweenSequenceHandle2D largeHandle{};
    ASSERT_TRUE(largeSequences.Create(largeDefinition, largeHandle).Succeeded());
    ASSERT_TRUE(largeSequences.Step(runtime::TweenTimeDomain2D::Presentation, 35ns).Succeeded());

    EXPECT_FLOAT_EQ(largeScene.TryGet(largeFirst)->LocalTransform().rotationRadians, 10.0F);
    EXPECT_FLOAT_EQ(largeScene.TryGet(largeSecond)->LocalTransform().rotationRadians, 20.0F);
    EXPECT_FLOAT_EQ(largeScene.TryGet(largeThird)->LocalTransform().rotationRadians, 10.0F);
    EXPECT_FLOAT_EQ(largeScene.TryGet(largeInserted)->LocalTransform().rotationRadians, 5.0F);
}

TEST(TweenSequence2DTests, SameBoundaryChildrenUseStableAuthorOrder)
{
    scene::ComponentRegistry registry{};
    scene::ComponentRegistration<OrderedProbe> registration{};
    registration.typeId = "game.ordered_probe";
    registration.tweenProperties.push_back({
        "first", runtime::TweenValueType2D::Float, ReadFirst, WriteFirst});
    registration.tweenProperties.push_back({
        "second", runtime::TweenValueType2D::Float, ReadSecond, WriteSecond});
    const auto probeType = registry.Register(std::move(registration));
    registry.Freeze();

    scene::Scene scene{registry};
    const scene::EntityId entity = scene.CreateEntity(Entity("ordered"));
    OrderedProbe& probe = scene.AddComponent(entity, probeType, OrderedProbe{});
    scene::TweenBindingSystem2D bindings{scene};

    scene::ResolvedTweenBinding2D firstBinding{};
    scene::ResolvedTweenBinding2D secondBinding{};
    ASSERT_TRUE(bindings.ResolveComponent(entity, probeType, "first", firstBinding).Succeeded());
    ASSERT_TRUE(bindings.ResolveComponent(entity, probeType, "second", secondBinding).Succeeded());

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(definition.Append(firstBinding, FloatSpec(0.0F, 1.0F)).Succeeded());
    ASSERT_TRUE(definition.Join(secondBinding, FloatSpec(0.0F, 2.0F)).Succeeded());

    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());
    ASSERT_EQ(probe.orderCount, 2U);
    EXPECT_EQ(probe.order[0], 1);
    EXPECT_EQ(probe.order[1], 2);

    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 10ns).Succeeded());
    ASSERT_EQ(probe.orderCount, 4U);
    EXPECT_EQ(probe.order[2], 1);
    EXPECT_EQ(probe.order[3], 2);
}

TEST(TweenSequence2DTests, MixedDomainInfiniteAndOverlappingWriterDefinitionsFailClosed)
{
    scene::Scene scene{};
    const scene::EntityId entity = scene.CreateEntity(Entity("validation"));
    scene::TweenBindingSystem2D bindings{scene};
    const scene::ResolvedTweenBinding2D rotation = ResolveRotation(bindings, entity);

    scene::TweenSequenceDefinition2D mixed{runtime::TweenTimeDomain2D::Presentation};
    EXPECT_EQ(
        mixed.Append(rotation, FloatSpec(
            0.0F,
            1.0F,
            10ns,
            runtime::TweenTimeDomain2D::Simulation)).error,
        scene::TweenSequenceError2D::MixedTimeDomain);

    auto infiniteSpec = FloatSpec(0.0F, 1.0F);
    infiniteSpec.tween.infinite = true;
    scene::TweenSequenceDefinition2D infinite{runtime::TweenTimeDomain2D::Presentation};
    EXPECT_EQ(
        infinite.Append(rotation, infiniteSpec).error,
        scene::TweenSequenceError2D::InfiniteChildUnsupported);

    scene::TweenSequenceDefinition2D conflict{runtime::TweenTimeDomain2D::Presentation};
    ASSERT_TRUE(conflict.Append(rotation, FloatSpec(0.0F, 10.0F)).Succeeded());
    ASSERT_TRUE(conflict.Join(rotation, FloatSpec(10.0F, 20.0F)).Succeeded());
    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    EXPECT_EQ(sequences.Create(conflict, handle).error, scene::TweenSequenceError2D::ChildConflict);
}

TEST(TweenSequence2DTests, SimulationDomainIsExplicitAndNegativeDeltaFailsClosed)
{
    scene::Scene scene{};
    const scene::EntityId entity = scene.CreateEntity(Entity("simulation"));
    scene::TweenBindingSystem2D bindings{scene};
    const scene::ResolvedTweenBinding2D rotation = ResolveRotation(bindings, entity);

    scene::TweenSequenceDefinition2D definition{runtime::TweenTimeDomain2D::Simulation};
    ASSERT_TRUE(definition.Append(
        rotation,
        FloatSpec(0.0F, 10.0F, 10ns, runtime::TweenTimeDomain2D::Simulation)).Succeeded());

    scene::TweenSequenceSystem2D sequences{bindings};
    scene::TweenSequenceHandle2D handle{};
    ASSERT_TRUE(sequences.Create(definition, handle).Succeeded());
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 0.0F);

    scene::TweenSequenceState2D state{};
    ASSERT_TRUE(sequences.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.elapsed, 0ns);
    ASSERT_TRUE(sequences.Step(runtime::TweenTimeDomain2D::Simulation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(scene.TryGet(entity)->LocalTransform().rotationRadians, 5.0F);
    ASSERT_TRUE(sequences.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.elapsed, 5ns);

    EXPECT_EQ(
        sequences.Step(runtime::TweenTimeDomain2D::Simulation, -1ns).error,
        scene::TweenSequenceError2D::NegativeDelta);
}
} // namespace
