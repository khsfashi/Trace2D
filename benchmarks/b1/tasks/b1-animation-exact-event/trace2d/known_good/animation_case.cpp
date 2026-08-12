#include "animation_case.hpp"

#include <array>
#include <chrono>

namespace trace2d::benchmark_b1
{
using namespace std::chrono_literals;

bool BuildAnimationCase(AnimationCase& outCase)
{
    outCase.asset.regions.resize(3U);
    const std::array frames{
        runtime::SpriteAnimationFrame2D{0U, 100ms},
        runtime::SpriteAnimationFrame2D{1U, 150ms},
        runtime::SpriteAnimationFrame2D{2U, 250ms},
    };
    const std::array events{
        runtime::SpriteAnimationEvent2D{7U, 250ms, 0U},
    };
    return runtime::SpriteAnimationClip2D::Prepare(
        &outCase.asset,
        3U,
        frames,
        events,
        outCase.clip).Succeeded();
}
}
