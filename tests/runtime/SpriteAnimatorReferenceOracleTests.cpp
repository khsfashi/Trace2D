#include "reference/SpriteAnimatorReferenceModel.hpp"

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationEmission2D;
using trace2d::runtime::SpriteAnimationEvent2D;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimationSpeed2D;
using trace2d::runtime::SpriteAnimator2D;
using trace2d::runtime::SpriteAnimator2DError;
using trace2d::runtime::SpriteAnimator2DState;
using trace2d::testing::SpriteAnimatorReferenceModel;

constexpr std::uint64_t kBaseSeed = 0x5452414345324401ULL;
constexpr std::uint32_t kGeneratedCaseCount = 5'000U;

[[nodiscard]] std::uint64_t Mix64(std::uint64_t value) noexcept
{
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

class DeterministicRng final
{
public:
    explicit DeterministicRng(std::uint64_t seed) noexcept
        : state_(seed)
    {
    }

    [[nodiscard]] std::uint64_t Next() noexcept
    {
        state_ = Mix64(state_);
        return state_;
    }

    [[nodiscard]] std::uint32_t Range(std::uint32_t exclusiveMaximum) noexcept
    {
        return static_cast<std::uint32_t>(Next() % exclusiveMaximum);
    }

private:
    std::uint64_t state_{0};
};

struct Scenario final
{
    std::uint64_t seed{0};
    std::vector<SpriteAnimationFrame2D> frames{};
    std::vector<SpriteAnimationEvent2D> events{};
    std::chrono::nanoseconds startTime{0};
    SpriteAnimationLoopMode loopMode{SpriteAnimationLoopMode::Once};
    SpriteAnimationDirection direction{SpriteAnimationDirection::Forward};
    SpriteAnimationSpeed2D speed{};
    std::chrono::nanoseconds delta{0};
};

[[nodiscard]] Scenario GenerateScenario(std::uint32_t caseIndex)
{
    Scenario scenario{};
    scenario.seed = Mix64(kBaseSeed + caseIndex);
    DeterministicRng rng{scenario.seed};

    const std::uint32_t frameCount = 1U + rng.Range(6U);
    scenario.frames.reserve(frameCount);
    std::int64_t duration = 0;
    for (std::uint32_t frame = 0; frame < frameCount; ++frame)
    {
        const std::int64_t frameDuration =
            caseIndex % 13U == 0U ? 1 + rng.Range(3U) : 1 + rng.Range(12U);
        scenario.frames.push_back({frame, std::chrono::nanoseconds{frameDuration}});
        duration += frameDuration;
    }

    std::uint32_t eventCount = rng.Range(11U);
    if (caseIndex % 7U == 0U)
    {
        eventCount = std::max(eventCount, 1U);
    }
    if (caseIndex % 11U == 0U)
    {
        eventCount = std::max(eventCount, 2U);
    }

    scenario.events.reserve(eventCount);
    for (std::uint32_t event = 0; event < eventCount; ++event)
    {
        scenario.events.push_back({
            1U + caseIndex * 16U + event,
            std::chrono::nanoseconds{rng.Range(static_cast<std::uint32_t>(duration))},
            event,
        });
    }

    if (!scenario.events.empty() && caseIndex % 7U == 0U)
    {
        scenario.events[0].offset = 0ns;
    }
    if (scenario.events.size() >= 2U && caseIndex % 11U == 0U)
    {
        const auto sharedOffset = std::chrono::nanoseconds{
            rng.Range(static_cast<std::uint32_t>(duration))};
        scenario.events[0].offset = sharedOffset;
        scenario.events[1].offset = sharedOffset;
    }

    for (std::size_t index = scenario.events.size(); index > 1U; --index)
    {
        const std::size_t other = rng.Range(static_cast<std::uint32_t>(index));
        std::swap(scenario.events[index - 1U], scenario.events[other]);
    }

    scenario.loopMode = static_cast<SpriteAnimationLoopMode>(caseIndex % 3U);
    scenario.direction =
        ((caseIndex / 3U) % 2U) == 0U ? SpriteAnimationDirection::Forward : SpriteAnimationDirection::Reverse;
    scenario.speed =
        caseIndex % 17U == 0U
            ? SpriteAnimationSpeed2D{0U, 1U + rng.Range(5U)}
            : SpriteAnimationSpeed2D{1U + rng.Range(5U), 1U + rng.Range(5U)};
    scenario.startTime = std::chrono::nanoseconds{
        rng.Range(static_cast<std::uint32_t>(duration + 1))};
    scenario.delta = std::chrono::nanoseconds{
        caseIndex % 13U == 0U ? 150 : static_cast<std::int64_t>(rng.Range(151U))};
    return scenario;
}

[[nodiscard]] std::string DescribeScenario(std::uint32_t caseIndex, const Scenario& scenario)
{
    std::ostringstream stream{};
    stream << "seed=0x" << std::hex << scenario.seed << std::dec
           << " case=" << caseIndex
           << " start_ns=" << scenario.startTime.count()
           << " delta_ns=" << scenario.delta.count()
           << " loop=" << static_cast<int>(scenario.loopMode)
           << " direction=" << static_cast<int>(scenario.direction)
           << " speed=" << scenario.speed.numerator << '/' << scenario.speed.denominator
           << " frame_durations_ns=[";
    for (std::size_t index = 0; index < scenario.frames.size(); ++index)
    {
        if (index != 0U)
        {
            stream << ',';
        }
        stream << scenario.frames[index].duration.count();
    }
    stream << "] events=[";
    for (std::size_t index = 0; index < scenario.events.size(); ++index)
    {
        if (index != 0U)
        {
            stream << ',';
        }
        stream << '{' << scenario.events[index].eventId << '@'
               << scenario.events[index].offset.count() << '#'
               << scenario.events[index].authoredOrdinal << '}';
    }
    stream << ']';
    return stream.str();
}

struct PreparedScenario final
{
    SpriteAsset asset{};
    SpriteAnimationClip2D clip{};
    SpriteAnimator2DState initialState{};
};

[[nodiscard]] std::unique_ptr<PreparedScenario> PrepareScenario(const Scenario& scenario)
{
    auto prepared = std::make_unique<PreparedScenario>();
    prepared->asset.regions.resize(scenario.frames.size());
    const auto clipStatus = SpriteAnimationClip2D::Prepare(
        &prepared->asset,
        static_cast<std::uint32_t>(prepared->asset.regions.size()),
        scenario.frames,
        scenario.events,
        prepared->clip);
    EXPECT_TRUE(clipStatus.Succeeded());

    const auto stateStatus = MakeSpriteAnimator2DState(
        prepared->clip,
        scenario.startTime,
        SpriteAnimationPlaybackState::Playing,
        scenario.loopMode,
        scenario.direction,
        false,
        scenario.speed,
        prepared->initialState);
    EXPECT_TRUE(stateStatus.Succeeded());
    return prepared;
}

void ExpectTranscriptEqual(
    const std::vector<SpriteAnimationEmission2D>& expected,
    const std::vector<SpriteAnimationEmission2D>& actual,
    std::size_t actualCount)
{
    ASSERT_EQ(actualCount, expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        EXPECT_EQ(actual[index], expected[index]) << "emission index=" << index;
    }
}

TEST(SpriteAnimatorReferenceOracleTests, ProductionMatchesIndependentReferenceAcrossFiveThousandGeneratedCases)
{
    for (std::uint32_t caseIndex = 0; caseIndex < kGeneratedCaseCount; ++caseIndex)
    {
        const Scenario scenario = GenerateScenario(caseIndex);
        SCOPED_TRACE(DescribeScenario(caseIndex, scenario));

        const auto prepared = PrepareScenario(scenario);
        SpriteAnimatorReferenceModel reference{prepared->initialState};
        const std::vector<SpriteAnimationEmission2D> expected = reference.Advance(scenario.delta);

        SpriteAnimator2D production{};
        ASSERT_TRUE(production.RestoreState(prepared->initialState).Succeeded());
        std::vector<SpriteAnimationEmission2D> actual(expected.size() + 4U);
        const auto result = production.Advance(scenario.delta, actual);

        ASSERT_TRUE(result.Succeeded());
        ExpectTranscriptEqual(expected, actual, result.emissionCount);
        EXPECT_EQ(production.State(), reference.State());
    }
}

TEST(SpriteAnimatorReferenceOracleTests, GeneratedCapacityFailuresRemainTransactional)
{
    constexpr std::uint32_t capacityCaseCount = 1'000U;
    for (std::uint32_t caseIndex = 0; caseIndex < capacityCaseCount; ++caseIndex)
    {
        Scenario scenario = GenerateScenario(caseIndex + kGeneratedCaseCount);
        scenario.loopMode = SpriteAnimationLoopMode::Loop;
        scenario.speed = {1U + (caseIndex % 5U), 1U};
        scenario.delta = 150ns;
        if (scenario.events.empty())
        {
            scenario.events.push_back({
                80'000U + caseIndex,
                0ns,
                0U,
            });
        }

        SCOPED_TRACE(DescribeScenario(caseIndex + kGeneratedCaseCount, scenario));
        const auto prepared = PrepareScenario(scenario);
        SpriteAnimatorReferenceModel reference{prepared->initialState};
        const std::vector<SpriteAnimationEmission2D> expected = reference.Advance(scenario.delta);
        ASSERT_FALSE(expected.empty());

        SpriteAnimator2D production{};
        ASSERT_TRUE(production.RestoreState(prepared->initialState).Succeeded());
        const SpriteAnimator2DState before = production.State();
        const std::size_t capacity = expected.size() - 1U;
        std::vector<SpriteAnimationEmission2D> output(capacity);

        const auto result = production.Advance(scenario.delta, output);
        EXPECT_EQ(result.error, SpriteAnimator2DError::OutputCapacityExceeded);
        EXPECT_EQ(result.emissionCount, capacity);
        EXPECT_EQ(production.State(), before);
    }
}
} // namespace
