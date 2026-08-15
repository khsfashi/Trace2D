#include <trace2d/agent/Inspection.hpp>
#include <trace2d/render/CameraViewport2D.hpp>
#include <trace2d/ui/Ui.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] input::PointerState Pointer(const float x, const float y) noexcept
{
    return input::PointerState{.x = x, .y = y};
}

[[nodiscard]] input::InputControlState IdleButton() noexcept
{
    return input::InputControlState{};
}

[[nodiscard]] input::InputControlState PressedButton() noexcept
{
    return input::InputControlState{.held = true, .pressed = true};
}

[[nodiscard]] input::InputControlState HeldButton() noexcept
{
    return input::InputControlState{.held = true};
}

[[nodiscard]] input::InputControlState ReleasedButton() noexcept
{
    return input::InputControlState{.released = true};
}

[[nodiscard]] input::InputControlState ClickedInOneFrame() noexcept
{
    return input::InputControlState{.pressed = true, .released = true};
}

void AddButton(
    UiDocument& document,
    const char* id,
    const UiRect bounds,
    const bool enabled = true)
{
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = id,
            .kind = UiElementKind::Button,
            .bounds = bounds,
            .name = id,
            .enabled = enabled,
        }),
        UiActionResult::Success);
}

void AddTextInput(UiDocument& document, const char* id, const UiRect bounds)
{
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = id,
            .kind = UiElementKind::TextInput,
            .bounds = bounds,
            .name = id,
        }),
        UiActionResult::Success);
}
} // namespace

TEST(UiPointerTests, TopmostOverlapUsesReverseAuthoredPainterOrderAndHalfOpenBounds)
{
    UiDocument document(160U, 96U);
    AddButton(document, "back", UiRect{10U, 10U, 40U, 20U});
    AddButton(document, "front", UiRect{20U, 10U, 40U, 20U});

    const UiPointerRouteResult overlap = document.ApplyPointer(Pointer(25.0F, 15.0F), IdleButton());
    ASSERT_TRUE(overlap.Succeeded());
    ASSERT_NE(document.HoveredElement(), nullptr);
    EXPECT_EQ(document.HoveredElement()->id, "front");
    EXPECT_FALSE(document.Find("back")->hovered);
    EXPECT_TRUE(document.Find("front")->hovered);

    // x == 60 is outside front's [20, 60) half-open edge and outside back's [10, 50).
    const UiPointerRouteResult edge = document.ApplyPointer(Pointer(60.0F, 15.0F), IdleButton());
    EXPECT_TRUE(edge.Succeeded());
    EXPECT_EQ(document.HoveredElement(), nullptr);
    EXPECT_FALSE(document.Find("front")->hovered);
}

TEST(UiPointerTests, CaptureSurvivesDragAndReleaseOutsideCancelsThenInsideReleaseActivates)
{
    UiDocument document(160U, 96U);
    AddButton(document, "drag", UiRect{20U, 20U, 60U, 24U});

    const UiPointerRouteResult press = document.ApplyPointer(Pointer(30.0F, 30.0F), PressedButton());
    ASSERT_TRUE(press.Succeeded());
    EXPECT_TRUE(press.consumed);
    ASSERT_NE(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.CapturedElement()->id, "drag");
    EXPECT_TRUE(document.Find("drag")->pointerPressed);
    EXPECT_TRUE(document.IsFocused("drag"));

    const UiPointerRouteResult outside = document.ApplyPointer(Pointer(120.0F, 70.0F), HeldButton());
    EXPECT_TRUE(outside.consumed);
    EXPECT_EQ(document.HoveredElement(), nullptr);
    ASSERT_NE(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.CapturedElement()->id, "drag");
    EXPECT_TRUE(document.Find("drag")->pointerPressed);

    const UiPointerRouteResult cancelled = document.ApplyPointer(Pointer(120.0F, 70.0F), ReleasedButton());
    EXPECT_TRUE(cancelled.consumed);
    EXPECT_FALSE(cancelled.activated);
    EXPECT_EQ(document.CapturedElement(), nullptr);
    EXPECT_FALSE(document.Find("drag")->pointerPressed);
    EXPECT_EQ(document.Find("drag")->activationCount, 0U);

    EXPECT_TRUE(document.ApplyPointer(Pointer(30.0F, 30.0F), PressedButton()).consumed);
    EXPECT_TRUE(document.ApplyPointer(Pointer(120.0F, 70.0F), HeldButton()).consumed);
    EXPECT_TRUE(document.ApplyPointer(Pointer(30.0F, 30.0F), HeldButton()).consumed);
    const UiPointerRouteResult activated = document.ApplyPointer(Pointer(30.0F, 30.0F), ReleasedButton());
    EXPECT_TRUE(activated.activated);
    EXPECT_EQ(document.Find("drag")->activationCount, 1U);
    EXPECT_EQ(document.CapturedElement(), nullptr);
}

TEST(UiPointerTests, SameFrameClickAndSemanticAgentActivationShareOneCounterAuthority)
{
    UiDocument document(160U, 96U);
    AddButton(document, "start", UiRect{10U, 10U, 60U, 24U});

    const UiPointerRouteResult click = document.ApplyPointer(Pointer(20.0F, 20.0F), ClickedInOneFrame());
    ASSERT_TRUE(click.Succeeded());
    EXPECT_TRUE(click.consumed);
    EXPECT_TRUE(click.activated);
    ASSERT_NE(document.Find("start"), nullptr);
    EXPECT_EQ(document.Find("start")->activationCount, 1U);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiActionResponse semantic = facade.ActivateUi(agent::UiSelector{.id = "start"});
    ASSERT_TRUE(semantic.Succeeded());
    EXPECT_EQ(semantic.element->activationCount, 2U);
    EXPECT_EQ(document.Find("start")->activationCount, 2U);
}

TEST(UiPointerTests, TextInputCapturesFocusWithoutButtonActivation)
{
    UiDocument document(160U, 96U);
    AddTextInput(document, "chat", UiRect{10U, 10U, 100U, 24U});

    const UiPointerRouteResult press = document.ApplyPointer(Pointer(20.0F, 20.0F), PressedButton());
    EXPECT_TRUE(press.consumed);
    EXPECT_TRUE(document.IsFocused("chat"));
    ASSERT_NE(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.CapturedElement()->id, "chat");

    const UiPointerRouteResult release = document.ApplyPointer(Pointer(20.0F, 20.0F), ReleasedButton());
    EXPECT_TRUE(release.consumed);
    EXPECT_FALSE(release.activated);
    EXPECT_EQ(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.Find("chat")->activationCount, 0U);
}

TEST(UiPointerTests, DisabledTopmostInteractiveElementBlocksUnderlyingActivationWithoutCapturing)
{
    UiDocument document(160U, 96U);
    AddButton(document, "under", UiRect{10U, 10U, 80U, 30U});
    AddButton(document, "disabled_overlay", UiRect{10U, 10U, 80U, 30U}, false);

    const UiPointerRouteResult hover = document.ApplyPointer(Pointer(20.0F, 20.0F), IdleButton());
    ASSERT_TRUE(hover.Succeeded());
    ASSERT_NE(document.HoveredElement(), nullptr);
    EXPECT_EQ(document.HoveredElement()->id, "disabled_overlay");

    const UiPointerRouteResult press = document.ApplyPointer(Pointer(20.0F, 20.0F), PressedButton());
    EXPECT_TRUE(press.consumed);
    EXPECT_EQ(document.CapturedElement(), nullptr);
    EXPECT_FALSE(document.IsFocused("under"));
    EXPECT_EQ(document.Find("under")->activationCount, 0U);
}

TEST(UiPointerTests, AgentInspectionAndAssertionObserveHoverPressAndCaptureState)
{
    UiDocument document(160U, 96U);
    AddButton(document, "inspect", UiRect{10U, 10U, 60U, 24U});
    ASSERT_TRUE(document.ApplyPointer(Pointer(20.0F, 20.0F), PressedButton()).Succeeded());

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiQueryOneResult observed = facade.QueryOneUi(agent::UiSelector{.id = "inspect"});
    ASSERT_TRUE(observed.Succeeded());
    EXPECT_TRUE(observed.match->focused);
    EXPECT_TRUE(observed.match->hovered);
    EXPECT_TRUE(observed.match->pointerPressed);
    EXPECT_TRUE(observed.match->pointerCaptured);

    const agent::UiAssertionResult asserted = facade.AssertUi(
        agent::UiSelector{.id = "inspect"},
        agent::UiExpectedState{
            .focused = true,
            .hovered = true,
            .pointerPressed = true,
            .pointerCaptured = true,
            .activationCount = 0U,
        });
    EXPECT_TRUE(asserted.Succeeded());
}

TEST(UiPointerTests, NonFinitePointerIsRejectedWithoutMutatingCurrentInteraction)
{
    UiDocument document(160U, 96U);
    AddButton(document, "safe", UiRect{10U, 10U, 60U, 24U});
    ASSERT_TRUE(document.ApplyPointer(Pointer(20.0F, 20.0F), PressedButton()).Succeeded());
    ASSERT_NE(document.CapturedElement(), nullptr);

    const UiPointerRouteResult invalid = document.ApplyPointer(
        Pointer(std::numeric_limits<float>::quiet_NaN(), 20.0F),
        ReleasedButton());
    EXPECT_FALSE(invalid.Succeeded());
    EXPECT_EQ(invalid.status, UiPointerRouteStatus::InvalidPosition);
    ASSERT_NE(document.CapturedElement(), nullptr);
    EXPECT_EQ(document.CapturedElement()->id, "safe");
    EXPECT_TRUE(document.Find("safe")->pointerPressed);
    EXPECT_EQ(document.Find("safe")->activationCount, 0U);
}

TEST(UiPointerTests, PresentationPointerUsesViewportAuthorityBeforeLogicalRouting)
{
    UiDocument document(320U, 180U);
    AddButton(document, "center", UiRect{140U, 80U, 40U, 20U});

    render::Viewport2D viewport{};
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;
    viewport.scaleMode = render::ViewportScaleMode2D::Fit;
    const render::ViewportResolveResult2D resolvedViewport = render::ResolveViewport2D(viewport, 800U, 600U);
    ASSERT_TRUE(resolvedViewport.Succeeded());

    render::CameraFrameState2D current{};
    current.entity = scene::EntityId{1U, 1U};
    current.center = render::Float2{0.0F, 0.0F};
    current.verticalSize = 10.0F;
    const render::PresentationViewResult2D resolvedView = render::ResolvePresentationView2D(
        current,
        nullptr,
        resolvedViewport.viewport,
        render::PresentationSamplingMode2D::AuthoritativeCurrent);
    ASSERT_TRUE(resolvedView.Succeeded());

    const render::Float2 centerPresentation{400.0F, 300.0F};
    ASSERT_TRUE(render::IsPresentationPointInsideViewport(resolvedView.view, centerPresentation));
    const render::CoordinateConversionResult2D logical =
        render::PresentationToViewport(resolvedView.view, centerPresentation);
    ASSERT_TRUE(logical.Succeeded());
    EXPECT_FLOAT_EQ(logical.value.x, 160.0F);
    EXPECT_FLOAT_EQ(logical.value.y, 90.0F);

    const UiPointerRouteResult click = document.ApplyPointer(
        Pointer(logical.value.x, logical.value.y),
        ClickedInOneFrame());
    EXPECT_TRUE(click.activated);
    EXPECT_EQ(document.Find("center")->activationCount, 1U);

    // Fit-mode bar coordinates are rejected by #88 and therefore never reach UiDocument.
    const render::Float2 letterboxPresentation{400.0F, 50.0F};
    EXPECT_FALSE(render::IsPresentationPointInsideViewport(resolvedView.view, letterboxPresentation));
    EXPECT_EQ(document.Find("center")->activationCount, 1U);
}
} // namespace trace2d::ui
