#include <trace2d/agent/CameraInspection.hpp>
#include <trace2d/scene/Camera2D.hpp>
#include <trace2d/scene/Components.hpp>
#include <trace2d/scene/Scene.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
trace2d::scene::EntityDescriptor Entity(
    std::string semanticId,
    const float x = 0.0F,
    const float y = 0.0F)
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.transform.position = {x, y};
    return descriptor;
}

trace2d::scene::Camera2D Camera(
    const std::int32_t priority,
    std::string targetViewport = "main",
    const bool enabled = true,
    const float verticalSize = 10.0F)
{
    trace2d::scene::Camera2D camera{};
    camera.enabled = enabled;
    camera.priority = priority;
    camera.verticalSize = verticalSize;
    camera.targetViewport = std::move(targetViewport);
    return camera;
}

TEST(CameraInspectionTests, StandardSceneRegistrationExposesResolvedActiveCameraIdentityAndState)
{
    trace2d::scene::ComponentRegistry registry{};
    const trace2d::scene::SceneComponentTypes types =
        trace2d::scene::RegisterSceneComponents(registry);
    registry.Freeze();
    ASSERT_TRUE(types.camera.IsValid());

    trace2d::scene::Scene scene{registry};
    const auto root = scene.CreateEntity(Entity("root", 10.0F, -4.0F));
    const auto zCamera = scene.CreateEntity(Entity("z_camera", 1.0F, 2.0F));
    ASSERT_EQ(scene.SetParent(zCamera, root), trace2d::scene::HierarchyResult::Success);
    (void)scene.AddComponent(zCamera, types.camera, Camera(7, "main", true, 14.0F));

    const auto aCamera = scene.CreateEntity(Entity("a_camera", -2.0F, 3.0F));
    ASSERT_EQ(scene.SetParent(aCamera, root), trace2d::scene::HierarchyResult::Success);
    (void)scene.AddComponent(aCamera, types.camera, Camera(7, "main", true, 12.0F));

    const auto otherViewport = scene.CreateEntity(Entity("secondary_camera"));
    (void)scene.AddComponent(otherViewport, types.camera, Camera(100, "secondary"));

    const auto inspected = trace2d::agent::InspectActiveCameraViewport2D(
        &scene, types.camera, "main");
    ASSERT_TRUE(inspected.Succeeded());
    ASSERT_TRUE(inspected.snapshot.has_value());
    const auto& snapshot = *inspected.snapshot;

    EXPECT_EQ(snapshot.viewportSemanticId, "main");
    EXPECT_EQ(snapshot.cameraEntitySemanticId, "a_camera");
    EXPECT_EQ(snapshot.entityIndex, aCamera.index);
    EXPECT_EQ(snapshot.entityGeneration, aCamera.generation);
    EXPECT_EQ(snapshot.priority, 7);
    EXPECT_TRUE(snapshot.enabled);
    EXPECT_FLOAT_EQ(snapshot.verticalSize, 12.0F);
    EXPECT_EQ(snapshot.targetViewport, "main");
    EXPECT_FLOAT_EQ(snapshot.worldCenter.x, 8.0F);
    EXPECT_FLOAT_EQ(snapshot.worldCenter.y, -1.0F);
}

TEST(CameraInspectionTests, NoActiveCameraIsAnExplicitAgentError)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto types = trace2d::scene::RegisterSceneComponents(registry);
    registry.Freeze();
    trace2d::scene::Scene scene{registry};

    const auto disabled = scene.CreateEntity(Entity("disabled"));
    (void)scene.AddComponent(disabled, types.camera, Camera(50, "main", false));
    const auto secondary = scene.CreateEntity(Entity("secondary"));
    (void)scene.AddComponent(secondary, types.camera, Camera(100, "secondary"));

    const auto inspected = trace2d::agent::InspectActiveCameraViewport2D(
        &scene, types.camera, "main");
    EXPECT_FALSE(inspected.Succeeded());
    ASSERT_TRUE(inspected.error.has_value());
    EXPECT_EQ(inspected.error->code, trace2d::agent::CameraInspectionErrorCode::NoActiveCamera);
    EXPECT_FALSE(inspected.snapshot.has_value());
}

TEST(CameraInspectionTests, ReinspectionAfterDespawnDeterministicallySelectsTheNextCamera)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto types = trace2d::scene::RegisterSceneComponents(registry);
    registry.Freeze();
    trace2d::scene::Scene scene{registry};

    const auto primary = scene.CreateEntity(Entity("primary"));
    (void)scene.AddComponent(primary, types.camera, Camera(20, "main", true, 9.0F));
    const auto fallback = scene.CreateEntity(Entity("fallback"));
    (void)scene.AddComponent(fallback, types.camera, Camera(10, "main", true, 11.0F));

    const auto first = trace2d::agent::InspectActiveCameraViewport2D(
        &scene, types.camera, "main");
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(first.snapshot.has_value());
    EXPECT_EQ(first.snapshot->cameraEntitySemanticId, "primary");

    ASSERT_TRUE(scene.DestroyEntity(primary));

    const auto second = trace2d::agent::InspectActiveCameraViewport2D(
        &scene, types.camera, "main");
    ASSERT_TRUE(second.Succeeded());
    ASSERT_TRUE(second.snapshot.has_value());
    EXPECT_EQ(second.snapshot->cameraEntitySemanticId, "fallback");
    EXPECT_EQ(second.snapshot->entityIndex, fallback.index);
    EXPECT_EQ(second.snapshot->entityGeneration, fallback.generation);
    EXPECT_FLOAT_EQ(second.snapshot->verticalSize, 11.0F);
}

TEST(CameraInspectionTests, InvalidBindingsAreStructuredErrors)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto types = trace2d::scene::RegisterSceneComponents(registry);
    registry.Freeze();
    trace2d::scene::Scene scene{registry};

    const auto noScene = trace2d::agent::InspectActiveCameraViewport2D(
        nullptr, types.camera, "main");
    ASSERT_TRUE(noScene.error.has_value());
    EXPECT_EQ(noScene.error->code, trace2d::agent::CameraInspectionErrorCode::SceneUnavailable);

    const auto emptyViewport = trace2d::agent::InspectActiveCameraViewport2D(
        &scene, types.camera, "");
    ASSERT_TRUE(emptyViewport.error.has_value());
    EXPECT_EQ(emptyViewport.error->code, trace2d::agent::CameraInspectionErrorCode::InvalidViewport);

    const trace2d::scene::ComponentTypeHandle<trace2d::scene::Camera2D> invalidType{};
    const auto invalidHandle = trace2d::agent::InspectActiveCameraViewport2D(
        &scene, invalidType, "main");
    ASSERT_TRUE(invalidHandle.error.has_value());
    EXPECT_EQ(invalidHandle.error->code, trace2d::agent::CameraInspectionErrorCode::InvalidCameraType);
}
} // namespace
