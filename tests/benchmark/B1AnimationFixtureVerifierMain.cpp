#include "animation_case.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

namespace
{
[[nodiscard]] bool VerifyAnimation()
{
    using namespace std::chrono_literals;

    trace2d::benchmark_b1::AnimationCase fixture{};
    if (!trace2d::benchmark_b1::BuildAnimationCase(fixture))
    {
        return false;
    }

    const auto frames = fixture.clip.Frames();
    const auto events = fixture.clip.Events();
    if (frames.size() != 3U || events.size() != 1U || fixture.clip.Duration() != 500ms)
    {
        return false;
    }
    if (frames[0] != trace2d::runtime::SpriteAnimationFrame2D{0U, 100ms} ||
        frames[1] != trace2d::runtime::SpriteAnimationFrame2D{1U, 150ms} ||
        frames[2] != trace2d::runtime::SpriteAnimationFrame2D{2U, 250ms})
    {
        return false;
    }
    if (events[0] != trace2d::runtime::SpriteAnimationEvent2D{7U, 250ms, 0U})
    {
        return false;
    }

    std::uint32_t frameIndex = 0U;
    const auto resolved = fixture.clip.ResolveFrameIndex(250ms, frameIndex);
    return resolved.Succeeded() && frameIndex == 2U;
}
}

int main()
{
    const bool accepted = VerifyAnimation();
    std::cout << (accepted ? "accepted" : "rejected") << '\n';
    return accepted ? 0 : 1;
}
