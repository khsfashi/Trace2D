#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

class NightfallLayoutAudit final
{
public:
    struct TextBounds final
    {
        std::string text{};
        float left{0.0F};
        float right{0.0F};
        float bottom{0.0F};
        float top{0.0F};
    };

    void Reset(
        const float safeLeft,
        const float safeRight,
        const float safeBottom,
        const float safeTop)
    {
        safeLeft_ = safeLeft;
        safeRight_ = safeRight;
        safeBottom_ = safeBottom;
        safeTop_ = safeTop;
        texts_.clear();
    }

    void Reserve(const std::size_t count)
    {
        texts_.reserve(count);
    }

    void RecordText(
        const std::string_view text,
        const float left,
        const float right,
        const float bottom,
        const float top)
    {
        texts_.push_back(TextBounds{
            std::string{text},
            std::min(left, right),
            std::max(left, right),
            std::min(bottom, top),
            std::max(bottom, top),
        });
    }

    [[nodiscard]] std::optional<std::string> Validate() const
    {
        constexpr float EdgeTolerance = 0.06F;
        constexpr float OverlapTolerance = 0.035F;

        for (const TextBounds& text : texts_)
        {
            if (!std::isfinite(text.left) || !std::isfinite(text.right) ||
                !std::isfinite(text.bottom) || !std::isfinite(text.top))
            {
                return std::string{"non-finite text bounds: "} + text.text;
            }
            if (text.left < safeLeft_ - EdgeTolerance || text.right > safeRight_ + EdgeTolerance ||
                text.bottom < safeBottom_ - EdgeTolerance || text.top > safeTop_ + EdgeTolerance)
            {
                std::ostringstream output{};
                output << "safe-area overflow: '" << text.text << "' bounds=["
                       << text.left << ',' << text.right << ',' << text.bottom << ',' << text.top
                       << "] safe=[" << safeLeft_ << ',' << safeRight_ << ',' << safeBottom_ << ','
                       << safeTop_ << ']';
                return output.str();
            }
        }

        for (std::size_t leftIndex = 0U; leftIndex < texts_.size(); ++leftIndex)
        {
            const TextBounds& left = texts_[leftIndex];
            for (std::size_t rightIndex = leftIndex + 1U; rightIndex < texts_.size(); ++rightIndex)
            {
                const TextBounds& right = texts_[rightIndex];
                const float overlapX = std::min(left.right, right.right) - std::max(left.left, right.left);
                const float overlapY = std::min(left.top, right.top) - std::max(left.bottom, right.bottom);
                if (overlapX > OverlapTolerance && overlapY > OverlapTolerance)
                {
                    std::ostringstream output{};
                    output << "text overlap: '" << left.text << "' <-> '" << right.text
                           << "' overlap=" << overlapX << 'x' << overlapY;
                    return output.str();
                }
            }
        }
        return std::nullopt;
    }

private:
    float safeLeft_{-8.0F};
    float safeRight_{8.0F};
    float safeBottom_{-4.5F};
    float safeTop_{4.5F};
    std::vector<TextBounds> texts_{};
};
