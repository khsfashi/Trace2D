#pragma once

#include <trace2d/ui/Ui.hpp>

#include <cstdint>
#include <vector>

namespace trace2d::ui
{
struct UiRasterImage final
{
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8{};
};

struct UiRasterMetrics final
{
    std::uint64_t elementsRasterized{0};
    std::uint64_t glyphsRasterized{0};

    [[nodiscard]] bool operator==(const UiRasterMetrics&) const noexcept = default;
};

[[nodiscard]] bool RasterizeUi(
    const UiDocument& document,
    UiRasterImage& output,
    UiRasterMetrics* metrics = nullptr);
} // namespace trace2d::ui
