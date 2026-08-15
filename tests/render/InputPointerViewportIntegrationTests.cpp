#include <trace2d/input/Input.hpp>
#include <trace2d/render/CameraViewport2D.hpp>

#include <gtest/gtest.h>

namespace
{
TEST(InputPointerViewportIntegrationTests, VirtualPointerUsesCameraViewportAuthorityAndRejectsFitBars)
{
    trace2d::input::InputSystem input{};
    trace2d::input::VirtualInputSource virtualInput{input};

    trace2d::render::Viewport2D viewport{};
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;
    viewport.scaleMode = trace2d::render::ViewportScaleMode2D::Fit;

    const auto resolvedViewport = trace2d::render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(resolvedViewport.Succeeded());

    trace2d::render::CameraFrameState2D current{};
    current.entity = trace2d::scene::EntityId{1U, 1U};
    current.center = trace2d::render::Float2{10.0F, -5.0F};
    current.verticalSize = 10.0F;

    const auto resolvedView = trace2d::render::ResolvePresentationView2D(
        current,
        nullptr,
        resolvedViewport.viewport,
        trace2d::render::PresentationSamplingMode2D::AuthoritativeCurrent);
    ASSERT_TRUE(resolvedView.Succeeded());

    virtualInput.MovePointer(400.0F, 300.0F, 400.0F, 300.0F);
    const trace2d::input::PointerState centerPointer = input.Pointer();
    const trace2d::render::Float2 centerPresentation{centerPointer.x, centerPointer.y};

    ASSERT_TRUE(trace2d::render::IsPresentationPointInsideViewport(
        resolvedView.view,
        centerPresentation));

    const auto centerViewport = trace2d::render::PresentationToViewport(
        resolvedView.view,
        centerPresentation);
    ASSERT_TRUE(centerViewport.Succeeded());
    EXPECT_FLOAT_EQ(centerViewport.value.x, 160.0F);
    EXPECT_FLOAT_EQ(centerViewport.value.y, 90.0F);

    const auto centerWorld = trace2d::render::PresentationToWorld(
        resolvedView.view,
        centerPresentation);
    ASSERT_TRUE(centerWorld.Succeeded());
    EXPECT_FLOAT_EQ(centerWorld.value.x, current.center.x);
    EXPECT_FLOAT_EQ(centerWorld.value.y, current.center.y);

    virtualInput.MovePointer(400.0F, 50.0F, 0.0F, -250.0F);
    const trace2d::input::PointerState barPointer = input.Pointer();
    const trace2d::render::Float2 barPresentation{barPointer.x, barPointer.y};

    EXPECT_FALSE(trace2d::render::IsPresentationPointInsideViewport(
        resolvedView.view,
        barPresentation));
}
} // namespace
