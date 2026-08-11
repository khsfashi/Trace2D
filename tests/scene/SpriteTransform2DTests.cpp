#include <trace2d/scene/SpriteTransform2D.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>

namespace trace2d::scene
{
namespace
{
SpritePose2D MakePose(
    const float x,
    const float y,
    const float rotation,
    const float scaleX,
    const float scaleY,
    const bool flipX = false,
    const bool flipY = false)
{
    SpritePose2D pose{};
    pose.transform.position = Vector2{x, y};
    pose.transform.rotationRadians = rotation;
    pose.transform.scale = Vector2{scaleX, scaleY};
    pose.flipX = flipX;
    pose.flipY = flipY;
    return pose;
}

TEST(SpriteTransform2DTests, SnapSynchronizesHistoryForDiscontinuities)
{
    SpritePoseHistory2D history{};
    const SpritePose2D pose = MakePose(4.0F, -3.0F, 0.25F, 2.0F, -1.0F, true, false);

    ASSERT_TRUE(SnapSpritePoseHistory(history, pose).Succeeded());
    EXPECT_EQ(history.previousFixed, pose);
    EXPECT_EQ(history.currentFixed, pose);
}

TEST(SpriteTransform2DTests, CommitMovesOldCurrentToPreviousOnlyWhenExplicitlyCalled)
{
    SpritePoseHistory2D history{};
    const SpritePose2D initial = MakePose(1.0F, 2.0F, 0.0F, 1.0F, 1.0F);
    ASSERT_TRUE(SnapSpritePoseHistory(history, initial).Succeeded());

    const SpritePose2D proposed = MakePose(8.0F, 9.0F, 1.0F, 3.0F, 4.0F, true, true);
    const SpritePoseHistory2D beforeCommit = history;
    ASSERT_TRUE(ValidateSpritePose(proposed).Succeeded());
    EXPECT_EQ(history, beforeCommit);

    ASSERT_TRUE(CommitSpriteFixedPose(history, proposed).Succeeded());
    EXPECT_EQ(history.previousFixed, initial);
    EXPECT_EQ(history.currentFixed, proposed);
}

TEST(SpriteTransform2DTests, InvalidCommitLeavesHistoryUnchanged)
{
    SpritePoseHistory2D history{};
    ASSERT_TRUE(SnapSpritePoseHistory(history, MakePose(1.0F, 2.0F, 0.0F, 1.0F, 1.0F)).Succeeded());
    const SpritePoseHistory2D before = history;

    SpritePose2D invalid = history.currentFixed;
    invalid.transform.position.x = std::numeric_limits<float>::infinity();
    const SpritePoseStatus status = CommitSpriteFixedPose(history, invalid);

    EXPECT_EQ(status, (SpritePoseStatus{SpritePoseError::NonFiniteTransform, SpritePoseField::Position}));
    EXPECT_EQ(history, before);
}

TEST(SpriteTransform2DTests, AuthoritativeCurrentCopiesCurrentExactly)
{
    SpritePoseHistory2D history{};
    history.previousFixed = MakePose(-10.0F, 2.0F, -1.0F, 4.0F, 5.0F, false, false);
    history.currentFixed = MakePose(7.0F, 11.0F, 7.5F, -2.0F, 0.0F, true, true);

    SpritePose2D presentation{};
    ASSERT_TRUE(ResolveSpriteAuthoritativeCurrent(history, presentation).Succeeded());
    EXPECT_EQ(presentation, history.currentFixed);
}

TEST(SpriteTransform2DTests, InterpolatesContinuousFieldsButUsesCurrentDiscreteFlips)
{
    SpritePoseHistory2D history{};
    history.previousFixed = MakePose(0.0F, 10.0F, 0.0F, 2.0F, -4.0F, false, true);
    history.currentFixed = MakePose(10.0F, -10.0F, 1.0F, 6.0F, 4.0F, true, false);

    SpritePose2D presentation{};
    ASSERT_TRUE(InterpolateSpritePose(history, 0.25F, presentation).Succeeded());
    EXPECT_FLOAT_EQ(presentation.transform.position.x, 2.5F);
    EXPECT_FLOAT_EQ(presentation.transform.position.y, 5.0F);
    EXPECT_FLOAT_EQ(presentation.transform.rotationRadians, 0.25F);
    EXPECT_FLOAT_EQ(presentation.transform.scale.x, 3.0F);
    EXPECT_FLOAT_EQ(presentation.transform.scale.y, -2.0F);
    EXPECT_TRUE(presentation.flipX);
    EXPECT_FALSE(presentation.flipY);
}

TEST(SpriteTransform2DTests, ShortestArcCrossesPiBoundary)
{
    constexpr float DegreesToRadians = std::numbers::pi_v<float> / 180.0F;
    SpritePoseHistory2D history{};
    history.previousFixed = MakePose(0.0F, 0.0F, 170.0F * DegreesToRadians, 1.0F, 1.0F);
    history.currentFixed = MakePose(0.0F, 0.0F, -170.0F * DegreesToRadians, 1.0F, 1.0F);

    SpritePose2D presentation{};
    ASSERT_TRUE(InterpolateSpritePose(history, 0.5F, presentation).Succeeded());
    EXPECT_NEAR(presentation.transform.rotationRadians, std::numbers::pi_v<float>, 1.0e-5F);
}

TEST(SpriteTransform2DTests, PiTiePreservesWrappedDifferenceSign)
{
    SpritePose2D presentation{};

    SpritePoseHistory2D positive{};
    positive.previousFixed = MakePose(0.0F, 0.0F, 0.0F, 1.0F, 1.0F);
    positive.currentFixed = MakePose(0.0F, 0.0F, std::numbers::pi_v<float>, 1.0F, 1.0F);
    ASSERT_TRUE(InterpolateSpritePose(positive, 0.5F, presentation).Succeeded());
    EXPECT_NEAR(presentation.transform.rotationRadians, std::numbers::pi_v<float> * 0.5F, 1.0e-6F);

    SpritePoseHistory2D negative{};
    negative.previousFixed = MakePose(0.0F, 0.0F, 0.0F, 1.0F, 1.0F);
    negative.currentFixed = MakePose(0.0F, 0.0F, -std::numbers::pi_v<float>, 1.0F, 1.0F);
    ASSERT_TRUE(InterpolateSpritePose(negative, 0.5F, presentation).Succeeded());
    EXPECT_NEAR(presentation.transform.rotationRadians, -std::numbers::pi_v<float> * 0.5F, 1.0e-6F);
}

TEST(SpriteTransform2DTests, AlphaEndpointsPreserveContinuousSamplesExactlyAndCurrentFlips)
{
    SpritePoseHistory2D history{};
    history.previousFixed = MakePose(1.0F, 2.0F, 7.0F, -3.0F, 0.0F, false, false);
    history.currentFixed = MakePose(9.0F, 8.0F, -8.0F, 5.0F, -6.0F, true, true);

    SpritePose2D atZero{};
    ASSERT_TRUE(InterpolateSpritePose(history, 0.0F, atZero).Succeeded());
    EXPECT_EQ(atZero.transform, history.previousFixed.transform);
    EXPECT_TRUE(atZero.flipX);
    EXPECT_TRUE(atZero.flipY);

    SpritePose2D atOne{};
    ASSERT_TRUE(InterpolateSpritePose(history, 1.0F, atOne).Succeeded());
    EXPECT_EQ(atOne, history.currentFixed);
}

TEST(SpriteTransform2DTests, RejectsInvalidAlphaInsteadOfClamping)
{
    SpritePoseHistory2D history{};
    ASSERT_TRUE(SnapSpritePoseHistory(history, MakePose(0.0F, 0.0F, 0.0F, 1.0F, 1.0F)).Succeeded());
    SpritePose2D output{};

    EXPECT_EQ(
        InterpolateSpritePose(history, -0.01F, output),
        (SpritePoseStatus{SpritePoseError::AlphaOutOfRange, SpritePoseField::Alpha}));
    EXPECT_EQ(
        InterpolateSpritePose(history, 1.01F, output),
        (SpritePoseStatus{SpritePoseError::AlphaOutOfRange, SpritePoseField::Alpha}));
    EXPECT_EQ(
        InterpolateSpritePose(history, std::numeric_limits<float>::quiet_NaN(), output),
        (SpritePoseStatus{SpritePoseError::NonFiniteAlpha, SpritePoseField::Alpha}));
}

TEST(SpriteTransform2DTests, ZeroAndNegativeScaleAreValidAuthoritativeSemantics)
{
    EXPECT_TRUE(ValidateSpritePose(MakePose(0.0F, 0.0F, 0.0F, 0.0F, -2.0F, true, false)).Succeeded());
}
} // namespace
} // namespace trace2d::scene
