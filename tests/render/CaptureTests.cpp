#include <trace2d/render/Capture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace
{
TEST(CaptureTests, ReadbackLayoutUsesD3D12FriendlyAlignedRows)
{
    trace2d::render::CaptureReadbackLayout layout{};

    ASSERT_TRUE(trace2d::render::TryBuildCaptureReadbackLayout(65, 2, layout));
    EXPECT_EQ(layout.pixelsPerRow, 128U);
    EXPECT_EQ(layout.rowPitchBytes, 512U);
    EXPECT_EQ(layout.transferBufferBytes, 1024U);
}

TEST(CaptureTests, ReadbackLayoutRejectsInvalidOrOversizedTargets)
{
    trace2d::render::CaptureReadbackLayout layout{};

    EXPECT_FALSE(trace2d::render::TryBuildCaptureReadbackLayout(0, 1, layout));
    EXPECT_EQ(layout, trace2d::render::CaptureReadbackLayout{});

    EXPECT_FALSE(trace2d::render::TryBuildCaptureReadbackLayout(
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max(),
        layout));
    EXPECT_EQ(layout, trace2d::render::CaptureReadbackLayout{});
}

TEST(CaptureTests, BmpArtifactIsDeterministicTopDownBgra)
{
    const std::filesystem::path path =
        std::filesystem::current_path() / "trace2d_capture_deterministic_test.bmp";
    std::error_code removeError{};
    std::filesystem::remove(path, removeError);

    trace2d::render::CaptureRequest request{};
    request.simulationFrame = 42;
    request.artifactPath = path;

    trace2d::render::CapturedFrame frame{};
    frame.simulationFrame = 42;
    frame.width = 1;
    frame.height = 1;
    frame.rgba8Pixels = std::vector<std::uint8_t>{1, 2, 3, 4};

    trace2d::render::WriteCaptureArtifact(request, frame);

    std::ifstream stream{path, std::ios::binary};
    ASSERT_TRUE(stream.is_open());
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};

    ASSERT_EQ(bytes.size(), 58U);
    EXPECT_EQ(bytes[0], static_cast<std::uint8_t>('B'));
    EXPECT_EQ(bytes[1], static_cast<std::uint8_t>('M'));
    EXPECT_EQ(bytes[2], 58U);
    EXPECT_EQ(bytes[10], 54U);
    EXPECT_EQ(bytes[18], 1U);
    EXPECT_EQ(bytes[22], 0xFFU);
    EXPECT_EQ(bytes[23], 0xFFU);
    EXPECT_EQ(bytes[24], 0xFFU);
    EXPECT_EQ(bytes[25], 0xFFU);
    EXPECT_EQ(bytes[28], 32U);
    EXPECT_EQ(bytes[54], 3U);
    EXPECT_EQ(bytes[55], 2U);
    EXPECT_EQ(bytes[56], 1U);
    EXPECT_EQ(bytes[57], 4U);

    std::filesystem::remove(path, removeError);
}

TEST(CaptureTests, ArtifactRejectsMismatchedSimulationFrame)
{
    trace2d::render::CaptureRequest request{};
    request.simulationFrame = 7;
    request.artifactPath = "unused.bmp";

    trace2d::render::CapturedFrame frame{};
    frame.simulationFrame = 8;
    frame.width = 1;
    frame.height = 1;
    frame.rgba8Pixels = std::vector<std::uint8_t>{0, 0, 0, 255};

    EXPECT_THROW(trace2d::render::WriteCaptureArtifact(request, frame), std::invalid_argument);
}
} // namespace
