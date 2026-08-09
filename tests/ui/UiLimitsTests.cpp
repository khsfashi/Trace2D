#include <trace2d/ui/Ui.hpp>
#include <trace2d/ui/UiRaster.hpp>

#include <gtest/gtest.h>

namespace trace2d::ui
{
namespace
{
TEST(UiLimitsTests, CanvasDimensionLimitRejectsOversizedDocumentsBeforeRasterAllocation)
{
    UiDocument maximum{MaxUiCanvasDimension, MaxUiCanvasDimension};
    EXPECT_TRUE(maximum.HasValidSize());

    UiDocument oversizedWidth{MaxUiCanvasDimension + 1U, 1U};
    EXPECT_FALSE(oversizedWidth.HasValidSize());

    UiElement element{};
    element.id = "oversized";
    element.kind = UiElementKind::Panel;
    element.bounds = UiRect{0U, 0U, 1U, 1U};
    EXPECT_EQ(oversizedWidth.AddElement(element), UiActionResult::InvalidDocumentSize);

    UiRasterImage image{};
    EXPECT_FALSE(RasterizeUi(oversizedWidth, image));
    EXPECT_TRUE(image.rgba8.empty());
}
} // namespace
} // namespace trace2d::ui
