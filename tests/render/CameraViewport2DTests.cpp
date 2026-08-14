#include <trace2d/render/CameraViewport2D.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

namespace
{
using trace2d::render::ActiveCameraSelectionError2D;
using trace2d::render::CameraFrameState2D;
using trace2d::render::Float2;
using trace2d::render::PresentationSamplingMode2D;
using trace2d::render::PresentationViewError2D;
using trace2d::render::Viewport2D;
using trace2d::render::ViewportScaleMode2D;

trace2d::scene::EntityDescriptor Entity(std::string semanticId, const float x = 0.0F, const float y = 0.0F)
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.transform.position = {x, y};
    return descriptor;
}

trace2d::scene::Camera2D Camera(
    const std::int32_t priority,
    std::string viewport = "main",
    const bool enabled = true,
    const float verticalSize = 10.0F)
{
    trace2d::scene::Camera2D camera{};
    camera.enabled = enabled;
    camera.priority = priority;
    camera.verticalSize = verticalSize;
    camera.targetViewport = std::move(viewport);
    return camera;
}

trace2d::scene::SemanticValue BoolValue(const bool value)
{
    trace2d::scene::SemanticValue semantic{};
    semantic.kind = trace2d::scene::SemanticValueKind::Boolean;
    semantic.booleanValue = value;
    return semantic;
}

trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue semantic{};
    semantic.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    semantic.signedIntegerValue = value;
    return semantic;
}

trace2d::scene::SemanticValue FloatValue(const double value)
{
    trace2d::scene::SemanticValue semantic{};
    semantic.kind = trace2d::scene::SemanticValueKind::Float;
    semantic.floatValue = value;
    return semantic;
}

trace2d::scene::SemanticValue TextValue(std::string value)
{
    trace2d::scene::SemanticValue semantic{};
    semantic.kind = trace2d::scene::SemanticValueKind::Text;
    semantic.textValue = std::move(value);
    return semantic;
}

TEST(CameraViewport2DTests, CameraIsAuthoredTypedStateAndGenericInspectionSeesIt)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto cameraType = trace2d::scene::RegisterCamera2DComponent(registry);
    registry.Freeze();

    trace2d::scene::Scene scene{registry};
    const auto entity = scene.CreateEntity(Entity("main_camera"));

    trace2d::scene::ComponentAuthoringObject authored{};
    authored.fields.push_back({"target_viewport", TextValue("main")});
    authored.fields.push_back({"vertical_size", FloatValue(12.0)});
    authored.fields.push_back({"priority", IntValue(7)});
    authored.fields.push_back({"enabled", BoolValue(true)});

    std::string error{};
    ASSERT_EQ(
        scene.AddAuthoredComponent(entity, "trace2d.camera2d", 1U, authored, error),
        trace2d::scene::ComponentAttachResult::Success)
        << error;

    const auto* camera = scene.TryGetComponent(entity, cameraType);
    ASSERT_NE(camera, nullptr);
    EXPECT_TRUE(camera->enabled);
    EXPECT_EQ(camera->priority, 7);
    EXPECT_FLOAT_EQ(camera->verticalSize, 12.0F);
    EXPECT_EQ(camera->targetViewport, "main");

    const auto inspected = scene.InspectComponents(entity);
    ASSERT_EQ(inspected.size(), 1U);
    EXPECT_EQ(inspected[0].typeId, "trace2d.camera2d");
    EXPECT_EQ(inspected[0].schemaVersion, 1U);
    ASSERT_EQ(inspected[0].fields.size(), 4U);
    EXPECT_EQ(inspected[0].fields[0].name, "enabled");
    EXPECT_EQ(inspected[0].fields[1].name, "priority");
    EXPECT_EQ(inspected[0].fields[2].name, "vertical_size");
    EXPECT_EQ(inspected[0].fields[3].name, "target_viewport");
}

TEST(CameraViewport2DTests, CameraAuthoringRejectsInvalidCanonicalSize)
{
    trace2d::scene::ComponentRegistry registry{};
    (void)trace2d::scene::RegisterCamera2DComponent(registry);
    registry.Freeze();

    trace2d::scene::Scene scene{registry};
    const auto entity = scene.CreateEntity(Entity("bad_camera"));
    trace2d::scene::ComponentAuthoringObject authored{};
    authored.fields.push_back({"enabled", BoolValue(true)});
    authored.fields.push_back({"priority", IntValue(0)});
    authored.fields.push_back({"vertical_size", FloatValue(0.0)});
    authored.fields.push_back({"target_viewport", TextValue("main")});

    std::string error{};
    EXPECT_EQ(
        scene.AddAuthoredComponent(entity, "trace2d.camera2d", 1U, authored, error),
        trace2d::scene::ComponentAttachResult::ValidationFailed);
    EXPECT_FALSE(error.empty());
}

TEST(CameraViewport2DTests, ActiveSelectionUsesPriorityThenStableSemanticId)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto cameraType = trace2d::scene::RegisterCamera2DComponent(registry);
    registry.Freeze();
    trace2d::scene::Scene scene{registry};

    const auto zCamera = scene.CreateEntity(Entity("z_camera"));
    (void)scene.AddComponent(zCamera, cameraType, Camera(10));
    const auto aCamera = scene.CreateEntity(Entity("a_camera"));
    (void)scene.AddComponent(aCamera, cameraType, Camera(10));
    const auto lower = scene.CreateEntity(Entity("lower_camera"));
    (void)scene.AddComponent(lower, cameraType, Camera(9));
    const auto disabled = scene.CreateEntity(Entity("disabled_camera"));
    (void)scene.AddComponent(disabled, cameraType, Camera(100, "main", false));
    const auto otherViewport = scene.CreateEntity(Entity("other_camera"));
    (void)scene.AddComponent(otherViewport, cameraType, Camera(100, "secondary"));

    Viewport2D viewport{};
    const auto selected = trace2d::render::ResolveActiveCamera2D(scene, cameraType, viewport);
    ASSERT_TRUE(selected.Succeeded());
    EXPECT_EQ(selected.camera.entity, aCamera);
    EXPECT_EQ(selected.camera.priority, 10);

    scene.TryGetComponent(aCamera, cameraType)->priority = 8;
    const auto selectedAgain = trace2d::render::ResolveActiveCamera2D(scene, cameraType, viewport);
    ASSERT_TRUE(selectedAgain.Succeeded());
    EXPECT_EQ(selectedAgain.camera.entity, zCamera);
}

TEST(CameraViewport2DTests, NoCameraIsExplicitAndNeverReusesPreviousSelection)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto cameraType = trace2d::scene::RegisterCamera2DComponent(registry);
    registry.Freeze();
    trace2d::scene::Scene scene{registry};

    const auto entity = scene.CreateEntity(Entity("secondary_camera"));
    (void)scene.AddComponent(entity, cameraType, Camera(1, "secondary"));

    Viewport2D viewport{};
    const auto selected = trace2d::render::ResolveActiveCamera2D(scene, cameraType, viewport);
    EXPECT_EQ(selected.error, ActiveCameraSelectionError2D::NoActiveCamera);
    EXPECT_FALSE(selected.camera.IsValid());
}

TEST(CameraViewport2DTests, CachedSelectionResolvesWorldPositionWithoutStringLookupAndGoesStale)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto cameraType = trace2d::scene::RegisterCamera2DComponent(registry);
    registry.Freeze();
    trace2d::scene::Scene scene{registry};

    const auto root = scene.CreateEntity(Entity("root", 10.0F, -3.0F));
    const auto cameraEntity = scene.CreateEntity(Entity("camera", 2.0F, 5.0F));
    ASSERT_EQ(scene.SetParent(cameraEntity, root), trace2d::scene::HierarchyResult::Success);
    (void)scene.AddComponent(cameraEntity, cameraType, Camera(1, "main", true, 14.0F));

    const auto selection = trace2d::render::ResolveActiveCamera2D(scene, cameraType, Viewport2D{});
    ASSERT_TRUE(selection.Succeeded());
    const auto frameState = trace2d::render::ResolveCameraFrameState2D(scene, cameraType, selection.camera);
    ASSERT_TRUE(frameState.Succeeded());
    EXPECT_FLOAT_EQ(frameState.state.center.x, 12.0F);
    EXPECT_FLOAT_EQ(frameState.state.center.y, 2.0F);
    EXPECT_FLOAT_EQ(frameState.state.verticalSize, 14.0F);

    ASSERT_TRUE(scene.DestroyEntity(cameraEntity));
    const auto stale = trace2d::render::ResolveCameraFrameState2D(scene, cameraType, selection.camera);
    EXPECT_EQ(stale.error, trace2d::render::CameraFrameStateError2D::StaleSelection);
}

TEST(CameraViewport2DTests, FitFillAndStretchResolveDeterministicCenteredMappings)
{
    Viewport2D viewport{};
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;

    viewport.scaleMode = ViewportScaleMode2D::Fit;
    const auto fit = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(fit.Succeeded());
    EXPECT_FLOAT_EQ(fit.viewport.viewportToPresentationScale.x, 2.5F);
    EXPECT_FLOAT_EQ(fit.viewport.viewportToPresentationScale.y, 2.5F);
    EXPECT_FLOAT_EQ(fit.viewport.contentRect.origin.x, 0.0F);
    EXPECT_FLOAT_EQ(fit.viewport.contentRect.origin.y, 75.0F);
    EXPECT_FLOAT_EQ(fit.viewport.contentRect.size.x, 800.0F);
    EXPECT_FLOAT_EQ(fit.viewport.contentRect.size.y, 450.0F);

    viewport.scaleMode = ViewportScaleMode2D::Fill;
    const auto fill = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(fill.Succeeded());
    EXPECT_NEAR(fill.viewport.viewportToPresentationScale.x, 10.0F / 3.0F, 1.0e-6F);
    EXPECT_NEAR(fill.viewport.viewportToPresentationScale.y, 10.0F / 3.0F, 1.0e-6F);
    EXPECT_NEAR(fill.viewport.contentRect.origin.x, -400.0F / 3.0F, 1.0e-4F);
    EXPECT_FLOAT_EQ(fill.viewport.contentRect.origin.y, 0.0F);
    EXPECT_NEAR(fill.viewport.contentRect.size.x, 3200.0F / 3.0F, 1.0e-4F);
    EXPECT_FLOAT_EQ(fill.viewport.contentRect.size.y, 600.0F);

    viewport.scaleMode = ViewportScaleMode2D::Stretch;
    const auto stretch = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(stretch.Succeeded());
    EXPECT_FLOAT_EQ(stretch.viewport.viewportToPresentationScale.x, 2.5F);
    EXPECT_NEAR(stretch.viewport.viewportToPresentationScale.y, 10.0F / 3.0F, 1.0e-6F);
    EXPECT_EQ(stretch.viewport.contentRect.origin, (Float2{0.0F, 0.0F}));
    EXPECT_EQ(stretch.viewport.contentRect.size, (Float2{800.0F, 600.0F}));
}

TEST(CameraViewport2DTests, WorldViewportPresentationRoundTripsAreBackendIndependent)
{
    Viewport2D viewport{};
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;
    viewport.scaleMode = ViewportScaleMode2D::Fit;
    const auto resolvedViewport = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(resolvedViewport.Succeeded());

    CameraFrameState2D current{};
    current.entity = trace2d::scene::EntityId{3U, 2U};
    current.center = Float2{10.0F, -5.0F};
    current.verticalSize = 10.0F;
    const auto resolved = trace2d::render::ResolvePresentationView2D(
        current, nullptr, resolvedViewport.viewport);
    ASSERT_TRUE(resolved.Succeeded());

    EXPECT_EQ(trace2d::render::WorldToViewport(resolved.view, current.center), (Float2{160.0F, 90.0F}));
    EXPECT_EQ(trace2d::render::WorldToPresentation(resolved.view, current.center), (Float2{400.0F, 300.0F}));

    const Float2 world{12.25F, -2.75F};
    const Float2 viewportPoint = trace2d::render::WorldToViewport(resolved.view, world);
    const Float2 worldRoundTrip = trace2d::render::ViewportToWorld(resolved.view, viewportPoint);
    EXPECT_NEAR(worldRoundTrip.x, world.x, 1.0e-5F);
    EXPECT_NEAR(worldRoundTrip.y, world.y, 1.0e-5F);

    const Float2 presentation = trace2d::render::WorldToPresentation(resolved.view, world);
    const Float2 presentationRoundTrip = trace2d::render::PresentationToWorld(resolved.view, presentation);
    EXPECT_NEAR(presentationRoundTrip.x, world.x, 1.0e-5F);
    EXPECT_NEAR(presentationRoundTrip.y, world.y, 1.0e-5F);

    const Float2 logicalTopLeft{
        resolved.view.logicalView.center.x - resolved.view.logicalView.halfExtents.x,
        resolved.view.logicalView.center.y + resolved.view.logicalView.halfExtents.y,
    };
    const Float2 targetTopLeft = trace2d::render::WorldToPresentation(resolved.view, logicalTopLeft);
    EXPECT_NEAR(targetTopLeft.x, resolved.view.viewport.contentRect.origin.x, 1.0e-4F);
    EXPECT_NEAR(targetTopLeft.y, resolved.view.viewport.contentRect.origin.y, 1.0e-4F);
}

TEST(CameraViewport2DTests, ResolvedRendererCameraRebuildsTheExactSamePresentationView)
{
    Viewport2D viewport{};
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;
    viewport.scaleMode = ViewportScaleMode2D::Stretch;
    const auto resolvedViewport = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(resolvedViewport.Succeeded());

    CameraFrameState2D current{};
    current.entity = trace2d::scene::EntityId{1U, 1U};
    current.center = Float2{3.0F, 4.0F};
    current.verticalSize = 9.0F;
    const auto resolved = trace2d::render::ResolvePresentationView2D(
        current, nullptr, resolvedViewport.viewport);
    ASSERT_TRUE(resolved.Succeeded());

    trace2d::render::OrthographicView rebuilt{};
    ASSERT_TRUE(trace2d::render::TryBuildOrthographicView(
        resolved.view.rendererCamera,
        resolved.view.viewport.targetWidth,
        resolved.view.viewport.targetHeight,
        rebuilt));
    EXPECT_EQ(rebuilt, resolved.view.presentationView);

    const Float2 logicalRight{
        resolved.view.logicalView.center.x + resolved.view.logicalView.halfExtents.x,
        resolved.view.logicalView.center.y,
    };
    const Float2 clip = trace2d::render::WorldToClip(rebuilt, logicalRight);
    EXPECT_NEAR(clip.x, 1.0F, 1.0e-5F);
    EXPECT_NEAR(clip.y, 0.0F, 1.0e-5F);
}

TEST(CameraViewport2DTests, ExactCurrentIgnoresWallClockAlphaWhileInterpolationIsExplicit)
{
    Viewport2D viewport{};
    const auto resolvedViewport = trace2d::render::ResolveViewport2D(viewport, 1280U, 720U);
    ASSERT_TRUE(resolvedViewport.Succeeded());

    CameraFrameState2D previous{};
    previous.entity = trace2d::scene::EntityId{4U, 1U};
    previous.center = Float2{0.0F, 0.0F};
    previous.verticalSize = 8.0F;
    CameraFrameState2D current = previous;
    current.center = Float2{8.0F, 4.0F};
    current.verticalSize = 12.0F;

    const auto exact = trace2d::render::ResolvePresentationView2D(
        current,
        nullptr,
        resolvedViewport.viewport,
        PresentationSamplingMode2D::AuthoritativeCurrent,
        std::numeric_limits<float>::quiet_NaN());
    ASSERT_TRUE(exact.Succeeded());
    EXPECT_EQ(exact.view.rendererCamera.center, current.center);
    EXPECT_FLOAT_EQ(exact.view.rendererCamera.verticalSize, 12.0F);
    EXPECT_FLOAT_EQ(exact.view.interpolationAlpha, 1.0F);

    const auto interpolated = trace2d::render::ResolvePresentationView2D(
        current,
        &previous,
        resolvedViewport.viewport,
        PresentationSamplingMode2D::Interpolated,
        0.25F);
    ASSERT_TRUE(interpolated.Succeeded());
    EXPECT_EQ(interpolated.view.rendererCamera.center, (Float2{2.0F, 1.0F}));
    EXPECT_FLOAT_EQ(interpolated.view.rendererCamera.verticalSize, 9.0F);
    EXPECT_FLOAT_EQ(interpolated.view.interpolationAlpha, 0.25F);

    CameraFrameState2D wrongHistory = previous;
    wrongHistory.entity = trace2d::scene::EntityId{5U, 1U};
    const auto mismatch = trace2d::render::ResolvePresentationView2D(
        current,
        &wrongHistory,
        resolvedViewport.viewport,
        PresentationSamplingMode2D::Interpolated,
        0.5F);
    EXPECT_EQ(mismatch.error, PresentationViewError2D::CameraHistoryMismatch);

    const auto invalidAlpha = trace2d::render::ResolvePresentationView2D(
        current,
        &previous,
        resolvedViewport.viewport,
        PresentationSamplingMode2D::Interpolated,
        1.01F);
    EXPECT_EQ(invalidAlpha.error, PresentationViewError2D::InvalidInterpolationAlpha);
}

TEST(CameraViewport2DTests, FitBarsAreRejectedForFuturePointerRouting)
{
    Viewport2D viewport{};
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;
    viewport.scaleMode = ViewportScaleMode2D::Fit;
    const auto resolvedViewport = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(resolvedViewport.Succeeded());

    CameraFrameState2D current{};
    current.entity = trace2d::scene::EntityId{1U, 1U};
    current.verticalSize = 10.0F;
    const auto resolved = trace2d::render::ResolvePresentationView2D(
        current, nullptr, resolvedViewport.viewport);
    ASSERT_TRUE(resolved.Succeeded());

    EXPECT_FALSE(trace2d::render::IsPresentationPointInsideViewport(resolved.view, Float2{400.0F, 10.0F}));
    EXPECT_TRUE(trace2d::render::IsPresentationPointInsideViewport(resolved.view, Float2{400.0F, 300.0F}));
    EXPECT_FALSE(trace2d::render::IsPresentationPointInsideViewport(resolved.view, Float2{-1.0F, 300.0F}));
}

TEST(CameraViewport2DTests, InvalidViewportDimensionsFailWithoutGpuOrWindow)
{
    Viewport2D viewport{};
    viewport.logicalWidth = 0U;
    EXPECT_EQ(
        trace2d::render::ResolveViewport2D(viewport, 1280U, 720U).error,
        trace2d::render::ViewportResolveError2D::InvalidLogicalSize);

    viewport.logicalWidth = 320U;
    EXPECT_EQ(
        trace2d::render::ResolveViewport2D(viewport, 0U, 720U).error,
        trace2d::render::ViewportResolveError2D::InvalidTargetSize);
}
} // namespace
