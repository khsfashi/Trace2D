#include <trace2d/render/CameraViewport2D.hpp>
#include <trace2d/ui/UiLayout.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace trace2d::ui
{
namespace
{
TEST(UiLayoutTests, ResolvesParentLocalBoundsAndStableHierarchyInspection)
{
    UiLayoutTree tree{320U, 180U};
    tree.ReserveNodes(4U);

    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "label",
            .parentId = "panel",
            .localBounds = UiRect{3U, 4U, 40U, 12U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "root",
            .localBounds = UiRect{10U, 12U, 200U, 120U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "panel",
            .parentId = "root",
            .localBounds = UiRect{20U, 16U, 100U, 60U},
        }),
        UiLayoutResult::Success);

    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);
    EXPECT_TRUE(tree.IsFinalized());

    const UiResolvedLayoutNode* const root = tree.Find("root");
    const UiResolvedLayoutNode* const panel = tree.Find("panel");
    const UiResolvedLayoutNode* const label = tree.Find("label");
    ASSERT_NE(root, nullptr);
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(label, nullptr);

    EXPECT_EQ(root->parentIndex, InvalidUiLayoutIndex);
    EXPECT_EQ(root->depth, 0U);
    EXPECT_EQ(root->resolvedLocalBounds, root->localBounds);
    EXPECT_EQ(root->bounds, (UiRect{10U, 12U, 200U, 120U}));

    ASSERT_NE(panel->parentIndex, InvalidUiLayoutIndex);
    EXPECT_EQ(tree.Nodes()[panel->parentIndex].id, "root");
    EXPECT_EQ(panel->depth, 1U);
    EXPECT_EQ(panel->resolvedLocalBounds, panel->localBounds);
    EXPECT_EQ(panel->bounds, (UiRect{30U, 28U, 100U, 60U}));

    ASSERT_NE(label->parentIndex, InvalidUiLayoutIndex);
    EXPECT_EQ(tree.Nodes()[label->parentIndex].id, "panel");
    EXPECT_EQ(label->depth, 2U);
    EXPECT_EQ(label->localBounds, (UiRect{3U, 4U, 40U, 12U}));
    EXPECT_EQ(label->resolvedLocalBounds, label->localBounds);
    EXPECT_EQ(label->bounds, (UiRect{33U, 32U, 40U, 12U}));
    EXPECT_EQ(tree.Find("missing"), nullptr);
}

TEST(UiLayoutTests, ResolvesFixedAnchorsPivotAndSignedOffsets)
{
    UiLayoutTree tree{320U, 180U};
    tree.ReserveNodes(4U);

    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "centered",
            .parentId = "panel",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{512U, 512U},
                .pivot = UiNormalizedPoint{512U, 512U},
                .offsetX = 10,
                .offsetY = -5,
                .width = 60U,
                .height = 20U,
            },
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "panel",
            .localBounds = UiRect{40U, 20U, 200U, 100U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "corner",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{1024U, 1024U},
                .pivot = UiNormalizedPoint{1024U, 1024U},
                .offsetX = -8,
                .offsetY = -6,
                .width = 64U,
                .height = 24U,
            },
        }),
        UiLayoutResult::Success);

    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);

    const UiResolvedLayoutNode* const centered = tree.Find("centered");
    const UiResolvedLayoutNode* const corner = tree.Find("corner");
    ASSERT_NE(centered, nullptr);
    ASSERT_NE(corner, nullptr);

    EXPECT_EQ(centered->placementMode, UiLayoutPlacementMode::AnchoredFixed);
    EXPECT_EQ(centered->resolvedLocalBounds, (UiRect{80U, 35U, 60U, 20U}));
    EXPECT_EQ(centered->bounds, (UiRect{120U, 55U, 60U, 20U}));

    EXPECT_EQ(corner->resolvedLocalBounds, (UiRect{248U, 150U, 64U, 24U}));
    EXPECT_EQ(corner->bounds, corner->resolvedLocalBounds);
    EXPECT_EQ(ToString(UiLayoutPlacementMode::AnchoredFixed), "anchored_fixed");
}

TEST(UiLayoutTests, UsesExactFixedUnitRoundHalfUpForAnchorsAndPivots)
{
    UiLayoutTree tree{200U, 100U};
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "parent",
            .localBounds = UiRect{10U, 10U, 101U, 51U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "odd",
            .parentId = "parent",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{512U, 512U},
                .pivot = UiNormalizedPoint{512U, 512U},
                .width = 3U,
                .height = 3U,
            },
        }),
        UiLayoutResult::Success);

    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);
    const UiResolvedLayoutNode* const odd = tree.Find("odd");
    ASSERT_NE(odd, nullptr);

    // 101 * 0.5 and 51 * 0.5 resolve to 51/26; a 3px centered pivot resolves to 2px.
    EXPECT_EQ(odd->resolvedLocalBounds, (UiRect{49U, 24U, 3U, 3U}));
    EXPECT_EQ(odd->bounds, (UiRect{59U, 34U, 3U, 3U}));
}

TEST(UiLayoutTests, RejectsInvalidAnchorsPlacementModesAndEscapingAnchoredGeometry)
{
    UiLayoutTree invalidAnchor{64U, 64U};
    EXPECT_EQ(
        invalidAnchor.AddNode(UiLayoutNodeSpec{
            .id = "bad-anchor",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{
                    static_cast<std::uint16_t>(UiNormalizedUnit + 1U),
                    0U,
                },
                .width = 8U,
                .height = 8U,
            },
        }),
        UiLayoutResult::InvalidAnchor);

    UiLayoutTree invalidMode{64U, 64U};
    EXPECT_EQ(
        invalidMode.AddNode(UiLayoutNodeSpec{
            .id = "bad-mode",
            .placementMode = static_cast<UiLayoutPlacementMode>(255U),
        }),
        UiLayoutResult::InvalidPlacementMode);

    UiLayoutTree invalidSize{64U, 64U};
    EXPECT_EQ(
        invalidSize.AddNode(UiLayoutNodeSpec{
            .id = "bad-size",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .width = 0U,
                .height = 8U,
            },
        }),
        UiLayoutResult::InvalidBounds);

    UiLayoutTree negativeChild{64U, 64U};
    ASSERT_EQ(
        negativeChild.AddNode(UiLayoutNodeSpec{
            .id = "parent",
            .localBounds = UiRect{8U, 8U, 32U, 32U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        negativeChild.AddNode(UiLayoutNodeSpec{
            .id = "child",
            .parentId = "parent",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .pivot = UiNormalizedPoint{512U, 512U},
                .width = 16U,
                .height = 16U,
            },
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(negativeChild.Finalize(), UiLayoutResult::ChildOutsideParent);

    UiLayoutTree hugeOffset{64U, 64U};
    ASSERT_EQ(
        hugeOffset.AddNode(UiLayoutNodeSpec{
            .id = "root",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{512U, 512U},
                .offsetX = std::numeric_limits<std::int32_t>::max(),
                .width = 8U,
                .height = 8U,
            },
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(hugeOffset.Finalize(), UiLayoutResult::InvalidBounds);
    EXPECT_FALSE(hugeOffset.IsFinalized());
}

TEST(UiLayoutTests, LogicalLayoutDoesNotReflowWhenPresentationTargetChanges)
{
    UiLayoutTree tree{320U, 180U};
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{
            .id = "hud",
            .placementMode = UiLayoutPlacementMode::AnchoredFixed,
            .anchored = UiAnchoredPlacement{
                .anchor = UiNormalizedPoint{1024U, 0U},
                .pivot = UiNormalizedPoint{1024U, 0U},
                .offsetX = -12,
                .offsetY = 8,
                .width = 80U,
                .height = 20U,
            },
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);

    const UiResolvedLayoutNode* const hud = tree.Find("hud");
    ASSERT_NE(hud, nullptr);
    const UiRect logicalBounds = hud->bounds;
    EXPECT_EQ(logicalBounds, (UiRect{228U, 8U, 80U, 20U}));

    const render::Viewport2D viewport{
        .semanticId = "main",
        .logicalWidth = tree.Width(),
        .logicalHeight = tree.Height(),
        .scaleMode = render::ViewportScaleMode2D::Fit,
    };
    const render::ViewportResolveResult2D first = render::ResolveViewport2D(viewport, 640U, 360U);
    const render::ViewportResolveResult2D second = render::ResolveViewport2D(viewport, 1000U, 600U);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(first.viewport.logicalWidth, tree.Width());
    EXPECT_EQ(first.viewport.logicalHeight, tree.Height());
    EXPECT_EQ(second.viewport.logicalWidth, tree.Width());
    EXPECT_EQ(second.viewport.logicalHeight, tree.Height());
    EXPECT_NE(first.viewport.targetWidth, second.viewport.targetWidth);
    EXPECT_NE(first.viewport.targetHeight, second.viewport.targetHeight);

    // Presentation scaling is #88 state. The finalized logical UI rectangle is unchanged.
    EXPECT_EQ(tree.Find("hud")->bounds, logicalBounds);
}

TEST(UiLayoutTests, RejectsDuplicateUnknownAndSelfParentIdentity)
{
    UiLayoutTree duplicate{64U, 64U};
    ASSERT_EQ(
        duplicate.AddNode(UiLayoutNodeSpec{.id = "same", .localBounds = UiRect{0U, 0U, 8U, 8U}}),
        UiLayoutResult::Success);
    ASSERT_EQ(
        duplicate.AddNode(UiLayoutNodeSpec{.id = "same", .localBounds = UiRect{8U, 0U, 8U, 8U}}),
        UiLayoutResult::Success);
    EXPECT_EQ(duplicate.Finalize(), UiLayoutResult::DuplicateId);
    EXPECT_FALSE(duplicate.IsFinalized());

    UiLayoutTree unknown{64U, 64U};
    ASSERT_EQ(
        unknown.AddNode(UiLayoutNodeSpec{
            .id = "child",
            .parentId = "missing",
            .localBounds = UiRect{0U, 0U, 8U, 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(unknown.Finalize(), UiLayoutResult::UnknownParent);
    EXPECT_FALSE(unknown.IsFinalized());

    UiLayoutTree self{64U, 64U};
    ASSERT_EQ(
        self.AddNode(UiLayoutNodeSpec{
            .id = "self",
            .parentId = "self",
            .localBounds = UiRect{0U, 0U, 8U, 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(self.Finalize(), UiLayoutResult::SelfParent);
    EXPECT_FALSE(self.IsFinalized());
}

TEST(UiLayoutTests, RejectsCyclesAndEscapingChildGeometry)
{
    UiLayoutTree cycle{64U, 64U};
    ASSERT_EQ(
        cycle.AddNode(UiLayoutNodeSpec{
            .id = "a",
            .parentId = "b",
            .localBounds = UiRect{0U, 0U, 8U, 8U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        cycle.AddNode(UiLayoutNodeSpec{
            .id = "b",
            .parentId = "a",
            .localBounds = UiRect{0U, 0U, 8U, 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(cycle.Finalize(), UiLayoutResult::HierarchyCycle);
    EXPECT_FALSE(cycle.IsFinalized());

    UiLayoutTree outside{128U, 128U};
    ASSERT_EQ(
        outside.AddNode(UiLayoutNodeSpec{
            .id = "root",
            .localBounds = UiRect{10U, 10U, 40U, 40U},
        }),
        UiLayoutResult::Success);
    ASSERT_EQ(
        outside.AddNode(UiLayoutNodeSpec{
            .id = "child",
            .parentId = "root",
            .localBounds = UiRect{32U, 0U, 16U, 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(outside.Finalize(), UiLayoutResult::ChildOutsideParent);
    EXPECT_FALSE(outside.IsFinalized());
}

TEST(UiLayoutTests, RejectsInvalidCanvasAndBoundsWithoutUnsignedWraparound)
{
    UiLayoutTree invalidCanvas{0U, 64U};
    ASSERT_EQ(
        invalidCanvas.AddNode(UiLayoutNodeSpec{
            .id = "root",
            .localBounds = UiRect{0U, 0U, 8U, 8U},
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(invalidCanvas.Finalize(), UiLayoutResult::InvalidCanvasSize);

    UiLayoutTree invalidSize{64U, 64U};
    EXPECT_EQ(
        invalidSize.AddNode(UiLayoutNodeSpec{
            .id = "zero",
            .localBounds = UiRect{0U, 0U, 0U, 8U},
        }),
        UiLayoutResult::InvalidBounds);

    UiLayoutTree wrappedRoot{64U, 64U};
    ASSERT_EQ(
        wrappedRoot.AddNode(UiLayoutNodeSpec{
            .id = "root",
            .localBounds = UiRect{
                std::numeric_limits<std::uint32_t>::max(),
                0U,
                2U,
                8U,
            },
        }),
        UiLayoutResult::Success);
    EXPECT_EQ(wrappedRoot.Finalize(), UiLayoutResult::InvalidBounds);
    EXPECT_FALSE(wrappedRoot.IsFinalized());
}

TEST(UiLayoutTests, FinalizedTreeRejectsFurtherMutation)
{
    UiLayoutTree tree{64U, 64U};
    ASSERT_EQ(
        tree.AddNode(UiLayoutNodeSpec{.id = "root", .localBounds = UiRect{0U, 0U, 64U, 64U}}),
        UiLayoutResult::Success);
    ASSERT_EQ(tree.Finalize(), UiLayoutResult::Success);

    EXPECT_EQ(
        tree.AddNode(UiLayoutNodeSpec{.id = "late", .localBounds = UiRect{0U, 0U, 8U, 8U}}),
        UiLayoutResult::AlreadyFinalized);
    EXPECT_EQ(tree.Finalize(), UiLayoutResult::AlreadyFinalized);
    EXPECT_EQ(ToString(UiLayoutResult::HierarchyCycle), "hierarchy_cycle");
    EXPECT_EQ(ToString(UiLayoutResult::InvalidAnchor), "invalid_anchor");
}
} // namespace
} // namespace trace2d::ui
