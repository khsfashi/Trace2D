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

// Existing resource-free deterministic fixture. It remains source-compatible for documents without
// U13 Image specialization and returns false if a live Image requires canonical texture bytes.
[[nodiscard]] bool RasterizeUi(
    const UiDocument& document,
    UiRasterImage& output,
    UiRasterMetrics* metrics = nullptr);

// U13 resource-aware fixture. Image sampling resolves the retained generation-safe texture handle
// directly through #86 and never stores a UI-owned decoded texture or backend resource.
[[nodiscard]] bool RasterizeUi(
    const UiDocument& document,
    const assets::ResourceRegistry& resources,
    UiRasterImage& output,
    UiRasterMetrics* metrics = nullptr);
} // namespace trace2d::ui
