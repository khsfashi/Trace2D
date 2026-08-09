#include <trace2d/ui/UiRaster.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace trace2d::ui
{
namespace
{
struct Color final
{
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};
};

constexpr Color CanvasColor{16U, 18U, 24U, 255U};
constexpr Color PanelColor{28U, 32U, 42U, 255U};
constexpr Color TextColor{236U, 240U, 248U, 255U};
constexpr Color DisabledTextColor{140U, 144U, 152U, 255U};
constexpr Color ButtonColor{58U, 84U, 136U, 255U};
constexpr Color DisabledButtonColor{66U, 68U, 74U, 255U};
constexpr Color InputColor{20U, 23U, 30U, 255U};
constexpr Color BorderColor{154U, 166U, 188U, 255U};
constexpr Color FocusBorderColor{246U, 196U, 76U, 255U};

[[nodiscard]] char ToUpperAscii(const char character) noexcept
{
    if (character >= 'a' && character <= 'z')
    {
        return static_cast<char>(character - 'a' + 'A');
    }
    return character;
}

[[nodiscard]] std::array<std::uint8_t, 7> GlyphRows(const char rawCharacter) noexcept
{
    switch (ToUpperAscii(rawCharacter))
    {
    case ' ':
        return {0U, 0U, 0U, 0U, 0U, 0U, 0U};
    case 'A':
        return {14U, 17U, 17U, 31U, 17U, 17U, 17U};
    case 'B':
        return {30U, 17U, 17U, 30U, 17U, 17U, 30U};
    case 'C':
        return {14U, 17U, 16U, 16U, 16U, 17U, 14U};
    case 'D':
        return {30U, 17U, 17U, 17U, 17U, 17U, 30U};
    case 'E':
        return {31U, 16U, 16U, 30U, 16U, 16U, 31U};
    case 'F':
        return {31U, 16U, 16U, 30U, 16U, 16U, 16U};
    case 'G':
        return {14U, 17U, 16U, 23U, 17U, 17U, 15U};
    case 'H':
        return {17U, 17U, 17U, 31U, 17U, 17U, 17U};
    case 'I':
        return {31U, 4U, 4U, 4U, 4U, 4U, 31U};
    case 'J':
        return {7U, 2U, 2U, 2U, 18U, 18U, 12U};
    case 'K':
        return {17U, 18U, 20U, 24U, 20U, 18U, 17U};
    case 'L':
        return {16U, 16U, 16U, 16U, 16U, 16U, 31U};
    case 'M':
        return {17U, 27U, 21U, 21U, 17U, 17U, 17U};
    case 'N':
        return {17U, 25U, 21U, 19U, 17U, 17U, 17U};
    case 'O':
        return {14U, 17U, 17U, 17U, 17U, 17U, 14U};
    case 'P':
        return {30U, 17U, 17U, 30U, 16U, 16U, 16U};
    case 'Q':
        return {14U, 17U, 17U, 17U, 21U, 18U, 13U};
    case 'R':
        return {30U, 17U, 17U, 30U, 20U, 18U, 17U};
    case 'S':
        return {15U, 16U, 16U, 14U, 1U, 1U, 30U};
    case 'T':
        return {31U, 4U, 4U, 4U, 4U, 4U, 4U};
    case 'U':
        return {17U, 17U, 17U, 17U, 17U, 17U, 14U};
    case 'V':
        return {17U, 17U, 17U, 17U, 17U, 10U, 4U};
    case 'W':
        return {17U, 17U, 17U, 21U, 21U, 21U, 10U};
    case 'X':
        return {17U, 17U, 10U, 4U, 10U, 17U, 17U};
    case 'Y':
        return {17U, 17U, 10U, 4U, 4U, 4U, 4U};
    case 'Z':
        return {31U, 1U, 2U, 4U, 8U, 16U, 31U};
    case '0':
        return {14U, 17U, 19U, 21U, 25U, 17U, 14U};
    case '1':
        return {4U, 12U, 4U, 4U, 4U, 4U, 14U};
    case '2':
        return {14U, 17U, 1U, 2U, 4U, 8U, 31U};
    case '3':
        return {30U, 1U, 1U, 14U, 1U, 1U, 30U};
    case '4':
        return {2U, 6U, 10U, 18U, 31U, 2U, 2U};
    case '5':
        return {31U, 16U, 16U, 30U, 1U, 1U, 30U};
    case '6':
        return {14U, 16U, 16U, 30U, 17U, 17U, 14U};
    case '7':
        return {31U, 1U, 2U, 4U, 8U, 8U, 8U};
    case '8':
        return {14U, 17U, 17U, 14U, 17U, 17U, 14U};
    case '9':
        return {14U, 17U, 17U, 15U, 1U, 1U, 14U};
    case '-':
        return {0U, 0U, 0U, 31U, 0U, 0U, 0U};
    case '_':
        return {0U, 0U, 0U, 0U, 0U, 0U, 31U};
    case '.':
        return {0U, 0U, 0U, 0U, 0U, 12U, 12U};
    case ':':
        return {0U, 12U, 12U, 0U, 12U, 12U, 0U};
    default:
        return {14U, 17U, 1U, 2U, 4U, 0U, 4U};
    }
}

void PutPixel(
    UiRasterImage& output,
    const std::uint32_t x,
    const std::uint32_t y,
    const Color color) noexcept
{
    if (x >= output.width || y >= output.height)
    {
        return;
    }

    const std::size_t pixelIndex =
        static_cast<std::size_t>(y) * static_cast<std::size_t>(output.width) +
        static_cast<std::size_t>(x);
    const std::size_t byteIndex = pixelIndex * 4U;
    output.rgba8[byteIndex] = color.r;
    output.rgba8[byteIndex + 1U] = color.g;
    output.rgba8[byteIndex + 2U] = color.b;
    output.rgba8[byteIndex + 3U] = color.a;
}

void FillRect(UiRasterImage& output, const UiRect& rect, const Color color) noexcept
{
    const std::uint32_t endX = rect.x + rect.width;
    const std::uint32_t endY = rect.y + rect.height;
    for (std::uint32_t y = rect.y; y < endY; ++y)
    {
        for (std::uint32_t x = rect.x; x < endX; ++x)
        {
            PutPixel(output, x, y, color);
        }
    }
}

void DrawBorder(UiRasterImage& output, const UiRect& rect, const Color color) noexcept
{
    const std::uint32_t right = rect.x + rect.width - 1U;
    const std::uint32_t bottom = rect.y + rect.height - 1U;

    for (std::uint32_t x = rect.x; x <= right; ++x)
    {
        PutPixel(output, x, rect.y, color);
        PutPixel(output, x, bottom, color);
    }

    for (std::uint32_t y = rect.y; y <= bottom; ++y)
    {
        PutPixel(output, rect.x, y, color);
        PutPixel(output, right, y, color);
    }
}

[[nodiscard]] std::uint64_t TextWidth(const std::string_view text) noexcept
{
    if (text.empty())
    {
        return 0U;
    }

    return static_cast<std::uint64_t>(text.size()) * 6U - 1U;
}

[[nodiscard]] std::uint64_t DrawText(
    UiRasterImage& output,
    const UiRect& clip,
    const std::string_view text,
    const std::uint32_t startX,
    const std::uint32_t startY,
    const Color color) noexcept
{
    constexpr std::uint32_t GlyphWidth = 5U;
    constexpr std::uint32_t GlyphHeight = 7U;
    constexpr std::uint32_t GlyphAdvance = 6U;

    const std::uint32_t clipRight = clip.x + clip.width;
    const std::uint32_t clipBottom = clip.y + clip.height;
    std::uint32_t penX = startX;
    std::uint64_t glyphs = 0U;

    for (const char character : text)
    {
        if (penX >= clipRight || startY >= clipBottom)
        {
            break;
        }

        const std::array<std::uint8_t, GlyphHeight> rows = GlyphRows(character);
        if (character != ' ')
        {
            ++glyphs;
        }

        for (std::uint32_t row = 0U; row < GlyphHeight; ++row)
        {
            const std::uint32_t y = startY + row;
            if (y >= clipBottom)
            {
                break;
            }

            for (std::uint32_t column = 0U; column < GlyphWidth; ++column)
            {
                const std::uint32_t x = penX + column;
                if (x >= clipRight)
                {
                    break;
                }

                const std::uint8_t mask = static_cast<std::uint8_t>(1U << (4U - column));
                if ((rows[row] & mask) != 0U)
                {
                    PutPixel(output, x, y, color);
                }
            }
        }

        if (penX > std::numeric_limits<std::uint32_t>::max() - GlyphAdvance)
        {
            break;
        }
        penX += GlyphAdvance;
    }

    return glyphs;
}

[[nodiscard]] std::uint32_t CenteredTextX(const UiRect& bounds, const std::string_view text) noexcept
{
    const std::uint64_t textWidth = TextWidth(text);
    if (textWidth >= bounds.width)
    {
        return bounds.x;
    }

    return bounds.x + static_cast<std::uint32_t>((static_cast<std::uint64_t>(bounds.width) - textWidth) / 2U);
}

[[nodiscard]] std::uint32_t CenteredTextY(const UiRect& bounds) noexcept
{
    constexpr std::uint32_t GlyphHeight = 7U;
    if (bounds.height <= GlyphHeight)
    {
        return bounds.y;
    }

    return bounds.y + (bounds.height - GlyphHeight) / 2U;
}
} // namespace

bool RasterizeUi(
    const UiDocument& document,
    UiRasterImage& output,
    UiRasterMetrics* const metrics)
{
    if (!document.HasValidSize())
    {
        return false;
    }

    const std::size_t width = static_cast<std::size_t>(document.Width());
    const std::size_t height = static_cast<std::size_t>(document.Height());
    if (height != 0U && width > std::numeric_limits<std::size_t>::max() / height)
    {
        return false;
    }

    const std::size_t pixelCount = width * height;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4U)
    {
        return false;
    }

    const std::size_t byteCount = pixelCount * 4U;
    output.width = document.Width();
    output.height = document.Height();
    output.rgba8.resize(byteCount);

    const UiRect canvas{0U, 0U, document.Width(), document.Height()};
    FillRect(output, canvas, CanvasColor);

    UiRasterMetrics localMetrics{};
    for (const UiElement& element : document.Elements())
    {
        ++localMetrics.elementsRasterized;
        const Color textColor = element.enabled ? TextColor : DisabledTextColor;

        switch (element.kind)
        {
        case UiElementKind::Panel:
            FillRect(output, element.bounds, PanelColor);
            break;
        case UiElementKind::Label:
            localMetrics.glyphsRasterized += DrawText(
                output,
                element.bounds,
                element.text,
                element.bounds.x,
                CenteredTextY(element.bounds),
                textColor);
            break;
        case UiElementKind::Button:
            FillRect(output, element.bounds, element.enabled ? ButtonColor : DisabledButtonColor);
            DrawBorder(
                output,
                element.bounds,
                document.IsFocused(element.id) ? FocusBorderColor : BorderColor);
            localMetrics.glyphsRasterized += DrawText(
                output,
                element.bounds,
                element.text,
                CenteredTextX(element.bounds, element.text),
                CenteredTextY(element.bounds),
                textColor);
            break;
        case UiElementKind::TextInput:
            FillRect(output, element.bounds, InputColor);
            DrawBorder(
                output,
                element.bounds,
                document.IsFocused(element.id) ? FocusBorderColor : BorderColor);
            localMetrics.glyphsRasterized += DrawText(
                output,
                element.bounds,
                element.text,
                element.bounds.x + (element.bounds.width > 8U ? 4U : 0U),
                CenteredTextY(element.bounds),
                textColor);
            break;
        }
    }

    if (metrics != nullptr)
    {
        *metrics = localMetrics;
    }

    return true;
}
} // namespace trace2d::ui
