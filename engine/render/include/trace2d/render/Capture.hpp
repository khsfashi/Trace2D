#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace trace2d::render
{
enum class CaptureImageFormat : std::uint8_t
{
    Bmp,
};

struct CaptureRequest final
{
    std::uint64_t simulationFrame{0};
    std::filesystem::path artifactPath{};
    CaptureImageFormat format{CaptureImageFormat::Bmp};
};

struct CaptureReadbackLayout final
{
    std::uint32_t pixelsPerRow{0};
    std::uint32_t rowPitchBytes{0};
    std::uint32_t transferBufferBytes{0};

    [[nodiscard]] bool operator==(const CaptureReadbackLayout&) const noexcept = default;
};

struct CapturedFrame final
{
    std::uint64_t simulationFrame{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8Pixels{};
};

[[nodiscard]] bool TryBuildCaptureReadbackLayout(
    std::uint32_t width,
    std::uint32_t height,
    CaptureReadbackLayout& outLayout) noexcept;

void WriteCaptureArtifact(const CaptureRequest& request, const CapturedFrame& frame);
} // namespace trace2d::render
