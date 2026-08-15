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
    EXPECT_EQ(root->bounds, (UiRect{10U, 12U, 200U, 120U}));

    ASSERT_NE(panel->parentIndex, InvalidUiLayoutIndex);
    EXPECT_EQ(tree.Nodes()[panel->parentIndex].id, "root");
    EXPECT_EQ(panel->depth, 1U);
    EXPECT_EQ(panel->bounds, (UiRect{30U, 28U, 100U, 60U}));

    ASSERT_NE(label->parentIndex, InvalidUiLayoutIndex);
    EXPECT_EQ(tree.Nodes()[label->parentIndex].id, "panel");
    EXPECT_EQ(label->depth, 2U);
    EXPECT_EQ(label->localBounds, (UiRect{3U, 4U, 40U, 12U}));
    EXPECT_EQ(label->bounds, (UiRect{33U, 32U, 40U, 12U}));
    EXPECT_EQ(tree.Find("missing"), nullptr);
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
}
} // namespace
} // namespace trace2d::ui
