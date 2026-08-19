#include <trace2d/scene/Camera2D.hpp>
#include <trace2d/scene/TweenBinding2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d;

struct TweenProbe final
{
    std::array<float, 4U> tint{1.0F, 2.0F, 3.0F, 1.0F};
    std::uint32_t writeCount{0U};
};

[[nodiscard]] runtime::TweenValue2D ReadProbeTint(const TweenProbe& probe) noexcept
{
    return runtime::TweenValue2D::Color(
        probe.tint[0], probe.tint[1], probe.tint[2], probe.tint[3]);
}

[[nodiscard]] bool WriteProbeTint(
    TweenProbe& probe,
    const runtime::TweenValue2D& value) noexcept
{
    if (value.type != runtime::TweenValueType2D::Color)
    {
        return false;
    }
    probe.tint = value.components;
    ++probe.writeCount;
    return true;
}

[[nodiscard]] scene::EntityDescriptor Entity(std::string semanticId)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    return descriptor;
}

[[nodiscard]] scene::TweenBindingSpec2D FloatSpec(
    const float start,
    const float end,
    const runtime::TweenTime2D duration = 100ns,
    const runtime::TweenTimeDomain2D domain = runtime::TweenTimeDomain2D::Presentation)
{
    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = domain;
    spec.tween.duration = duration;
    spec.tween.start = runtime::TweenValue2D::Float(start);
    spec.tween.end = runtime::TweenValue2D::Float(end);
    return spec;
}

TEST(TweenBinding2DTests, ResolvedTransformBindingKeepsTimeDomainsSeparateAtRetainedCapacity)
{
    scene::Scene scene{};
    auto descriptor = Entity("player");
    descriptor.transform.position = {1.0F, 2.0F};
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));

    scene::TweenBindingSystem2D tweens{scene};
    tweens.Reserve(2U);
    scene::ResolvedTweenBinding2D position{};
    scene::ResolvedTweenBinding2D rotation{};
    ASSERT_TRUE(tweens.ResolveTransform(
        entity, scene::TransformTweenProperty2D::Position, position).Succeeded());
    ASSERT_TRUE(tweens.ResolveTransform(
        entity, scene::TransformTweenProperty2D::RotationRadians, rotation).Succeeded());
    EXPECT_EQ(position.valueType, runtime::TweenValueType2D::Float2);

    scene::TweenBindingSpec2D positionSpec{};
    positionSpec.tween.domain = runtime::TweenTimeDomain2D::Presentation;
    positionSpec.tween.duration = 100ns;
    positionSpec.tween.start = runtime::TweenValue2D::Float2(1.0F, 2.0F);
    positionSpec.tween.end = runtime::TweenValue2D::Float2(11.0F, 22.0F);
    runtime::TweenHandle2D positionHandle{};
    ASSERT_TRUE(tweens.Create(position, positionSpec, positionHandle).Succeeded());

    runtime::TweenHandle2D rotationHandle{};
    ASSERT_TRUE(tweens.Create(
        rotation,
        FloatSpec(0.0F, 1.0F, 100ns, runtime::TweenTimeDomain2D::Simulation),
        rotationHandle).Succeeded());

    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 50ns).Succeeded());
    const scene::Entity* stored = scene.TryGet(entity);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->LocalTransform().position.x, 6.0F);
    EXPECT_FLOAT_EQ(stored->LocalTransform().position.y, 12.0F);
    EXPECT_FLOAT_EQ(stored->LocalTransform().rotationRadians, 0.0F);

    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Simulation, 50ns).Succeeded());
    stored = scene.TryGet(entity);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored->LocalTransform().rotationRadians, 0.5F);
    EXPECT_GE(tweens.Metrics().retainedBindingCapacity, 2U);
}

TEST(TweenBinding2DTests, ExternalComponentOptsInThroughRegistrationAndCaptureSamplesOnce)
{
    scene::ComponentRegistry components{};

    scene::ComponentRegistration<TweenProbe> duplicateRegistration{};
    duplicateRegistration.typeId = "game.duplicate_probe";
    duplicateRegistration.tweenProperties.push_back({
        "tint", runtime::TweenValueType2D::Color, ReadProbeTint, WriteProbeTint});
    duplicateRegistration.tweenProperties.push_back({
        "tint", runtime::TweenValueType2D::Color, ReadProbeTint, WriteProbeTint});
    EXPECT_THROW((void)components.Register(std::move(duplicateRegistration)), std::invalid_argument);

    scene::ComponentRegistration<TweenProbe> probeRegistration{};
    probeRegistration.typeId = "game.tween_probe";
    probeRegistration.tweenProperties.push_back({
        "tint", runtime::TweenValueType2D::Color, ReadProbeTint, WriteProbeTint});
    const auto probeType = components.Register(std::move(probeRegistration));
    components.Freeze();

    scene::Scene scene{components};
    const scene::EntityId entity = scene.CreateEntity(Entity("external"));
    TweenProbe& probe = scene.AddComponent(entity, probeType, TweenProbe{});

    scene::TweenBindingSystem2D tweens{scene};
    scene::ResolvedTweenBinding2D binding{};
    ASSERT_TRUE(tweens.ResolveComponent(entity, probeType, "tint", binding).Succeeded());
    EXPECT_EQ(binding.componentType, probeType.Index());
    EXPECT_EQ(binding.valueType, runtime::TweenValueType2D::Color);

    scene::ResolvedTweenBinding2D semanticBinding{};
    ASSERT_TRUE(tweens.ResolveComponent(
        entity, "game.tween_probe", "tint", semanticBinding).Succeeded());
    EXPECT_EQ(semanticBinding, binding);

    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = runtime::TweenTimeDomain2D::Presentation;
    spec.tween.delay = 10ns;
    spec.tween.duration = 10ns;
    spec.tween.end = runtime::TweenValue2D::Color(1.0F, 1.0F, 1.0F, 0.0F);
    spec.startMode = scene::TweenStartMode2D::CaptureCurrent;
    spec.endMode = scene::TweenEndMode2D::Relative;

    runtime::TweenHandle2D handle{};
    ASSERT_TRUE(tweens.Create(binding, spec, handle).Succeeded());
    probe.tint = {2.0F, 3.0F, 4.0F, 1.0F};
    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_EQ(probe.writeCount, 0U);

    probe.tint = {3.0F, 4.0F, 5.0F, 0.5F};
    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_EQ(probe.writeCount, 1U);
    EXPECT_EQ(tweens.Metrics().capturedStartCount, 1U);

    probe.tint = {100.0F, 100.0F, 100.0F, 1.0F};
    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_EQ(probe.writeCount, 2U);
    EXPECT_FLOAT_EQ(probe.tint[0], 3.5F);
    EXPECT_FLOAT_EQ(probe.tint[1], 4.5F);
    EXPECT_FLOAT_EQ(probe.tint[2], 5.5F);
    EXPECT_FLOAT_EQ(probe.tint[3], 0.5F);
}

TEST(TweenBinding2DTests, RejectAndReplaceConflictsAreDeterministicAndInspectable)
{
    scene::Scene scene{};
    const scene::EntityId entity = scene.CreateEntity(Entity("conflict"));
    scene::TweenBindingSystem2D tweens{scene};

    scene::ResolvedTweenBinding2D binding{};
    ASSERT_TRUE(tweens.ResolveTransform(
        entity, scene::TransformTweenProperty2D::RotationRadians, binding).Succeeded());

    runtime::TweenHandle2D first{};
    ASSERT_TRUE(tweens.Create(binding, FloatSpec(0.0F, 10.0F), first).Succeeded());

    runtime::TweenHandle2D rejected{};
    EXPECT_EQ(
        tweens.Create(binding, FloatSpec(1.0F, 11.0F), rejected).error,
        scene::TweenBindingError2D::ConflictRejected);

    auto replacementSpec = FloatSpec(5.0F, 15.0F);
    replacementSpec.conflictPolicy = scene::TweenConflictPolicy2D::Replace;
    runtime::TweenHandle2D replacement{};
    ASSERT_TRUE(tweens.Create(binding, replacementSpec, replacement).Succeeded());

    runtime::TweenState2D firstState{};
    ASSERT_TRUE(tweens.Inspect(first, firstState).Succeeded());
    EXPECT_EQ(firstState.playback, runtime::TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(firstState.cancellationReason, runtime::TweenCancellationReason2D::Replaced);
    EXPECT_EQ(tweens.PoolMetrics().activeCount, 1U);
    EXPECT_EQ(tweens.Metrics().conflictRejectedCount, 1U);
    EXPECT_EQ(tweens.Metrics().conflictReplacedCount, 1U);

    EXPECT_EQ(tweens.Restart(first).error, scene::TweenBindingError2D::ConflictRejected);
}

TEST(TweenBinding2DTests, DestroyedAndRecycledEntityGenerationCancelsWithoutStaleWrite)
{
    scene::Scene scene{};
    const scene::EntityId oldEntity = scene.CreateEntity(Entity("old"));
    scene::TweenBindingSystem2D tweens{scene};

    scene::ResolvedTweenBinding2D binding{};
    ASSERT_TRUE(tweens.ResolveTransform(
        oldEntity, scene::TransformTweenProperty2D::Position, binding).Succeeded());

    scene::TweenBindingSpec2D spec{};
    spec.tween.domain = runtime::TweenTimeDomain2D::Presentation;
    spec.tween.duration = 10ns;
    spec.tween.start = runtime::TweenValue2D::Float2(0.0F, 0.0F);
    spec.tween.end = runtime::TweenValue2D::Float2(10.0F, 10.0F);
    runtime::TweenHandle2D handle{};
    ASSERT_TRUE(tweens.Create(binding, spec, handle).Succeeded());

    ASSERT_TRUE(scene.DestroyEntity(oldEntity));
    auto replacementDescriptor = Entity("new");
    replacementDescriptor.transform.position = {50.0F, 60.0F};
    const scene::EntityId newEntity = scene.CreateEntity(std::move(replacementDescriptor));
    EXPECT_EQ(newEntity.index, oldEntity.index);
    EXPECT_NE(newEntity.generation, oldEntity.generation);

    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    runtime::TweenState2D state{};
    ASSERT_TRUE(tweens.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, runtime::TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(state.cancellationReason, runtime::TweenCancellationReason2D::TargetInvalidated);
    EXPECT_EQ(tweens.Metrics().targetInvalidatedCount, 1U);

    const scene::Entity* const replacement = scene.TryGet(newEntity);
    ASSERT_NE(replacement, nullptr);
    EXPECT_FLOAT_EQ(replacement->LocalTransform().position.x, 50.0F);
    EXPECT_FLOAT_EQ(replacement->LocalTransform().position.y, 60.0F);
}

TEST(TweenBinding2DTests, CameraVerticalSizeUsesRegisteredPresentationPropertyAndFailsClosed)
{
    scene::ComponentRegistry components{};
    const auto cameraType = scene::RegisterCamera2DComponent(components);
    components.Freeze();

    scene::Scene scene{components};
    const scene::EntityId entity = scene.CreateEntity(Entity("camera"));
    scene::Camera2D& camera = scene.AddComponent(entity, cameraType, scene::Camera2D{});
    camera.verticalSize = 10.0F;

    scene::TweenBindingSystem2D tweens{scene};
    scene::ResolvedTweenBinding2D binding{};
    ASSERT_TRUE(tweens.ResolveComponent(
        entity, "trace2d.camera2d", "vertical_size", binding).Succeeded());

    runtime::TweenHandle2D handle{};
    ASSERT_TRUE(tweens.Create(binding, FloatSpec(10.0F, 20.0F, 10ns), handle).Succeeded());
    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 5ns).Succeeded());
    EXPECT_FLOAT_EQ(camera.verticalSize, 15.0F);

    ASSERT_TRUE(tweens.Cancel(handle).Succeeded());
    runtime::TweenHandle2D invalidFinal{};
    auto invalidSpec = FloatSpec(15.0F, -5.0F, 1ns);
    ASSERT_TRUE(tweens.Create(binding, invalidSpec, invalidFinal).Succeeded());
    ASSERT_TRUE(tweens.Step(runtime::TweenTimeDomain2D::Presentation, 1ns).Succeeded());

    runtime::TweenState2D invalidState{};
    ASSERT_TRUE(tweens.Inspect(invalidFinal, invalidState).Succeeded());
    EXPECT_EQ(invalidState.playback, runtime::TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(
        invalidState.cancellationReason,
        runtime::TweenCancellationReason2D::PropertyWriteRejected);
    EXPECT_FLOAT_EQ(camera.verticalSize, 15.0F);
    EXPECT_EQ(tweens.Metrics().propertyWriteRejectedCount, 1U);
}
} // namespace
