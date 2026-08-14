#pragma once

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

namespace trace2d::benchmark_b1
{
struct AnimationCase final
{
    assets::SpriteAsset asset{};
    runtime::SpriteAnimationClip2D clip{};
};

[[nodiscard]] bool BuildAnimationCase(AnimationCase& outCase);
}
