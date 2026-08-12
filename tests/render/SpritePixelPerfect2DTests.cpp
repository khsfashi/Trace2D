#include <trace2d/render/SpritePixelPerfect2D.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>

namespace trace2d::render
{
namespace
{
constexpr std::uint32_t LogicalWidth = 320U;
constexpr std::uint32_t LogicalHeight = 180U;
constexpr float PixelsPerUnit = 16.0F;

assets::SpriteAsset MakePixelAsset()
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/pixel-perfect.sprite.toml";
    asset.pages = {
        assets::SpriteAtlasPage{
            "main",
            "textures/pixel-perfect.png",
            assets::SpritePixelSize{16U, 16U},
            assets::SpriteColorSpace::Srgb,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "frame",
            "main",
            assets::SpritePixelSize{4U, 2U},
            assets::SpritePixelOffset{1U, 0U},
            assets::SpritePixelSize{2U, 2U},
            assets::SpritePixelRect{4U, 6U, 2U, 2U},
            assets::SpriteRationalPivot{0, 0, 1},
            assets::SpritePackedRotation::None,
        },
    };
    return asset;
}

ResolvedSpriteRegion ResolveOnlyRegion(const assets::SpriteAsset& asset)
{
    ResolvedSpriteRegion selection{};
    EXPECT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());
    return selection;
}

OrthographicCamera PixelCamera()
{
    OrthographicCamera camera{};
    camera.verticalSize = static_cast<float>(LogicalHeight) / PixelsPerUnit;
    return camera;
}

SpritePixelPerfectViewport2D BuildViewport(
    const OrthographicCamera& camera = PixelCamera(),
    const std::uint32_t targetWidth = 1280U,
    const std::uint32_t targetHeight = 720U)
{
    SpritePixelPerfectViewport2D viewport{};
    EXPECT_TRUE(BuildSpritePixelPerfectViewport(
        camera,
        LogicalWidth,
        LogicalHeight,
        targetWidth,
        targetHeight,
        viewport).Succeeded());
    return viewport;
}

scene::SpritePoseHistory2D HistoryAt(const scene::Vector2 position)
{
    scene::SpritePose2D pose{};
    pose.transform.position = position;
    return scene::SpritePoseHistory2D{pose, pose};
}

void ExpectNear(const Float2 actual, const float x, const float y, const float epsilon = 1.0e-4F)
{
    EXPECT_NEAR(actual.x, x, epsilon);
    EXPECT_NEAR(actual.y, y, epsilon);
}

TEST(SpritePixelPerfect2DTests, BuildsIntegerContainedViewportFromLogicalAspect)
{
    const SpritePixelPerfectViewport2D viewport = BuildViewport(PixelCamera(), 1366U, 768U);

    EXPECT_EQ(viewport.logicalWidth, LogicalWidth);
    EXPECT_EQ(viewport.logicalHeight, LogicalHeight);
    EXPECT_EQ(viewport.targetWidth, 1366U);
    EXPECT_EQ(viewport.targetHeight, 768U);
    EXPECT_EQ(viewport.integerScale, 4U);
    EXPECT_EQ(viewport.contentRect, (SpritePixelRect2D{43U, 24U, 1280U, 720U}));
    EXPECT_NEAR(
        viewport.logicalView.halfExtents.x / viewport.logicalView.halfExtents.y,
        static_cast<float>(LogicalWidth) / static_cast<float>(LogicalHeight),
        1.0e-5F);
}

TEST(SpritePixelPerfect2DTests, CentersOddRemaindersWithFloorOnLeftAndTop)
{
    const SpritePixelPerfectViewport2D viewport = BuildViewport(PixelCamera(), 1283U, 723U);

    EXPECT_EQ(viewport.integerScale, 4U);
    EXPECT_EQ(viewport.contentRect, (SpritePixelRect2D{1U, 1U, 1280U, 720U}));
    EXPECT_EQ(viewport.targetWidth - viewport.contentRect.x - viewport.contentRect.width, 2U);
    EXPECT_EQ(viewport.targetHeight - viewport.contentRect.y - viewport.contentRect.height, 2U);
}

TEST(SpritePixelPerfect2DTests, RejectsInvalidAndTooSmallViewportInputsTransactionally)
{
    SpritePixelPerfectViewport2D viewport{};
    viewport.logicalWidth = 999U;

    EXPECT_EQ(
        BuildSpritePixelPerfectViewport(PixelCamera(), 0U, LogicalHeight, 1280U, 720U, viewport),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidLogicalSize,
            SpritePixelPerfectField::LogicalViewport}));
    EXPECT_EQ(viewport, SpritePixelPerfectViewport2D{});

    EXPECT_EQ(
        BuildSpritePixelPerfectViewport(PixelCamera(), LogicalWidth, LogicalHeight, 319U, 180U, viewport),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::TargetTooSmall,
            SpritePixelPerfectField::Target}));
    EXPECT_EQ(viewport, SpritePixelPerfectViewport2D{});

    OrthographicCamera invalidCamera = PixelCamera();
    invalidCamera.verticalSize = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(
        BuildSpritePixelPerfectViewport(
            invalidCamera, LogicalWidth, LogicalHeight, 1280U, 720U, viewport),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidCamera,
            SpritePixelPerfectField::Camera}));
    EXPECT_EQ(viewport, SpritePixelPerfectViewport2D{});
}

TEST(SpritePixelPerfect2DTests, RejectsCorruptedPrecomputedViewport)
{
    SpritePixelPerfectViewport2D viewport = BuildViewport();
    viewport.contentRect.x += 1U;
    EXPECT_EQ(
        ValidateSpritePixelPerfectViewport(viewport),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidViewport,
            SpritePixelPerfectField::LogicalViewport}));

    viewport = BuildViewport();
    viewport.logicalView.clipScale.x *= 2.0F;
    EXPECT_EQ(
        ValidateSpritePixelPerfectViewport(viewport),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidViewport,
            SpritePixelPerfectField::View}));
}

TEST(SpritePixelPerfect2DTests, SnapsUntrimmedSourceOriginAndPreservesOnePixelBasis)
{
    const assets::SpriteAsset asset = MakePixelAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildViewport();
    const float quarterPixelWorld = 0.25F / PixelsPerUnit;
    const scene::SpritePoseHistory2D history =
        HistoryAt(scene::Vector2{quarterPixelWorld, -quarterPixelWorld});
    const scene::SpritePoseHistory2D originalHistory = history;

    SpritePixelPerfectMapping2D mapping{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection,
        history,
        PixelsPerUnit,
        viewport,
        SpritePixelPerfectPoseRequest{},
        mapping).Succeeded());

    ExpectNear(mapping.sourceOriginLogicalBeforeSnap, 160.25F, 90.25F);
    ExpectNear(mapping.sourceOriginLogicalAfterSnap, 160.0F, 90.0F);
    ExpectNear(mapping.sourcePixelBasisXLogical, 1.0F, 0.0F);
    ExpectNear(mapping.sourcePixelBasisYLogical, 0.0F, 1.0F);
    EXPECT_EQ(mapping.sourcePixelScaleX, 1U);
    EXPECT_EQ(mapping.sourcePixelScaleY, 1U);
    EXPECT_FALSE(mapping.axesSwapped);
    ExpectNear(
        mapping.worldSnapDelta,
        -quarterPixelWorld,
        quarterPixelWorld,
        1.0e-5F);
    EXPECT_EQ(history, originalHistory);
}

TEST(SpritePixelPerfect2DTests, SupportsIntegerMagnificationFlipsAndQuarterTurnAxisSwap)
{
    const assets::SpriteAsset asset = MakePixelAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildViewport();

    scene::SpritePose2D pose{};
    pose.transform.scale = scene::Vector2{2.0F, 2.0F};
    pose.flipX = true;
    scene::SpritePoseHistory2D history{pose, pose};
    SpritePixelPerfectMapping2D mapping{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection, history, PixelsPerUnit, viewport, {}, mapping).Succeeded());
    EXPECT_EQ(mapping.sourcePixelScaleX, 2U);
    EXPECT_EQ(mapping.sourcePixelScaleY, 2U);
    EXPECT_FALSE(mapping.axesSwapped);
    ExpectNear(mapping.sourcePixelBasisXLogical, -2.0F, 0.0F);
    ExpectNear(mapping.sourcePixelBasisYLogical, 0.0F, 2.0F);

    pose.flipX = false;
    pose.transform.scale = scene::Vector2{1.0F, 1.0F};
    pose.transform.rotationRadians = std::numbers::pi_v<float> * 0.5F;
    history = scene::SpritePoseHistory2D{pose, pose};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection, history, PixelsPerUnit, viewport, {}, mapping).Succeeded());
    EXPECT_TRUE(mapping.axesSwapped);
    EXPECT_EQ(mapping.sourcePixelScaleX, 1U);
    EXPECT_EQ(mapping.sourcePixelScaleY, 1U);
    ExpectNear(mapping.sourcePixelBasisXLogical, 0.0F, -1.0F);
    ExpectNear(mapping.sourcePixelBasisYLogical, -1.0F, 0.0F);
}

TEST(SpritePixelPerfect2DTests, RejectsFractionalScaleAndArbitraryRotationAsInexactGrid)
{
    const assets::SpriteAsset asset = MakePixelAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildViewport();
    SpritePixelPerfectMapping2D mapping{};

    scene::SpritePose2D pose{};
    pose.transform.scale.x = 1.5F;
    scene::SpritePoseHistory2D history{pose, pose};
    EXPECT_EQ(
        ResolveSpritePixelPerfectPose(selection, history, PixelsPerUnit, viewport, {}, mapping),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidSourcePixelGrid,
            SpritePixelPerfectField::SourcePixelBasis}));
    EXPECT_EQ(mapping, SpritePixelPerfectMapping2D{});

    pose = scene::SpritePose2D{};
    pose.transform.rotationRadians = std::numbers::pi_v<float> * 0.25F;
    history = scene::SpritePoseHistory2D{pose, pose};
    EXPECT_EQ(
        ResolveSpritePixelPerfectPose(selection, history, PixelsPerUnit, viewport, {}, mapping),
        (SpritePixelPerfectStatus{
            SpritePixelPerfectError::InvalidSourcePixelGrid,
            SpritePixelPerfectField::SourcePixelBasis}));
    EXPECT_EQ(mapping, SpritePixelPerfectMapping2D{});
}

TEST(SpritePixelPerfect2DTests, RationalPivotAndTrimDoNotReplaceUntrimmedSourceGridAnchor)
{
    assets::SpriteAsset asset = MakePixelAsset();
    asset.regions[0].pivot = assets::SpriteRationalPivot{1, 3, 2};
    asset.regions[0].trimOffset = assets::SpritePixelOffset{2U, 0U};
    asset.regions[0].trimSize = assets::SpritePixelSize{1U, 2U};
    asset.regions[0].packedRect = assets::SpritePixelRect{8U, 4U, 1U, 2U};
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildViewport();
    const scene::SpritePoseHistory2D history = HistoryAt(scene::Vector2{});

    SpritePixelPerfectMapping2D mapping{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection, history, PixelsPerUnit, viewport, {}, mapping).Succeeded());
    EXPECT_EQ(mapping.sourcePixelScaleX, 1U);
    EXPECT_EQ(mapping.sourcePixelScaleY, 1U);

    SpriteLogicalQuad snappedQuad{};
    ASSERT_TRUE(BuildSpriteLogicalQuad(
        selection, mapping.presentationPose, PixelsPerUnit, snappedQuad).Succeeded());
    // A non-integer rational pivot may shift the authoritative transform. The derived source
    // pixel-edge origin, not the pivot or trim rectangle, is what SR6 lands on the pixel grid.
    EXPECT_NEAR(mapping.sourceOriginLogicalAfterSnap.x, std::floor(mapping.sourceOriginLogicalAfterSnap.x), 1.0e-4F);
    EXPECT_NEAR(mapping.sourceOriginLogicalAfterSnap.y, std::floor(mapping.sourceOriginLogicalAfterSnap.y), 1.0e-4F);
}

TEST(SpritePixelPerfect2DTests, SelectsAuthoritativeCurrentOrExistingSr1InterpolationBeforeSnap)
{
    const assets::SpriteAsset asset = MakePixelAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildViewport();
    scene::SpritePose2D previous{};
    scene::SpritePose2D current{};
    current.transform.position.x = 3.0F / PixelsPerUnit;
    const scene::SpritePoseHistory2D history{previous, current};

    SpritePixelPerfectMapping2D exact{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection,
        history,
        PixelsPerUnit,
        viewport,
        SpritePixelPerfectPoseRequest{SpritePresentationTimeMode::AuthoritativeCurrent, 0.0F},
        exact).Succeeded());
    EXPECT_NEAR(exact.sourceOriginLogicalAfterSnap.x, 163.0F, 1.0e-4F);

    SpritePixelPerfectMapping2D interpolated{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection,
        history,
        PixelsPerUnit,
        viewport,
        SpritePixelPerfectPoseRequest{SpritePresentationTimeMode::Interpolated, 0.5F},
        interpolated).Succeeded());
    // 161.5 ties toward +infinity by the SR6 contract.
    EXPECT_NEAR(interpolated.sourceOriginLogicalBeforeSnap.x, 161.5F, 1.0e-4F);
    EXPECT_NEAR(interpolated.sourceOriginLogicalAfterSnap.x, 162.0F, 1.0e-4F);
}

TEST(SpritePixelPerfect2DTests, CommonCameraAndSpriteTranslationPreservesRelativeSnapPhase)
{
    const assets::SpriteAsset asset = MakePixelAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const float quarterPixelWorld = 0.25F / PixelsPerUnit;
    const scene::SpritePoseHistory2D baselineHistory =
        HistoryAt(scene::Vector2{quarterPixelWorld, 0.0F});
    const SpritePixelPerfectViewport2D baselineViewport = BuildViewport();

    SpritePixelPerfectMapping2D baseline{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection, baselineHistory, PixelsPerUnit, baselineViewport, {}, baseline).Succeeded());

    constexpr float CommonPixelTranslation = 7.0F;
    const float commonWorldTranslation = CommonPixelTranslation / PixelsPerUnit;
    OrthographicCamera movedCamera = PixelCamera();
    movedCamera.center.x = commonWorldTranslation;
    const SpritePixelPerfectViewport2D movedViewport = BuildViewport(movedCamera);
    const scene::SpritePoseHistory2D movedHistory = HistoryAt(
        scene::Vector2{quarterPixelWorld + commonWorldTranslation, 0.0F});

    SpritePixelPerfectMapping2D moved{};
    ASSERT_TRUE(ResolveSpritePixelPerfectPose(
        selection, movedHistory, PixelsPerUnit, movedViewport, {}, moved).Succeeded());

    EXPECT_NEAR(
        moved.sourceOriginLogicalBeforeSnap.x,
        baseline.sourceOriginLogicalBeforeSnap.x,
        1.0e-4F);
    EXPECT_NEAR(moved.worldSnapDelta.x, baseline.worldSnapDelta.x, 1.0e-5F);
    EXPECT_NEAR(
        moved.presentationPose.transform.position.x - baseline.presentationPose.transform.position.x,
        commonWorldTranslation,
        1.0e-5F);
}

TEST(SpritePixelPerfect2DTests, ReusesCallerOwnedMappingDeterministically)
{
    const assets::SpriteAsset asset = MakePixelAsset();
    const ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    const SpritePixelPerfectViewport2D viewport = BuildViewport();
    const scene::SpritePoseHistory2D history = HistoryAt(
        scene::Vector2{0.25F / PixelsPerUnit, -0.25F / PixelsPerUnit});
    SpritePixelPerfectMapping2D mapping{};

    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration)
    {
        ASSERT_TRUE(ResolveSpritePixelPerfectPose(
            selection, history, PixelsPerUnit, viewport, {}, mapping).Succeeded());
    }
    ExpectNear(mapping.sourceOriginLogicalAfterSnap, 160.0F, 90.0F);
}
} // namespace
} // namespace trace2d::render
