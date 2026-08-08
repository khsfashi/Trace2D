#include <trace2d/render/Capture.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace trace2d::render
{
namespace
{
constexpr std::uint64_t Rgba8BytesPerPixel = 4;
constexpr std::uint64_t ReadbackRowAlignmentBytes = 256;
constexpr std::size_t BmpFileHeaderBytes = 14;
constexpr std::size_t BmpInfoHeaderBytes = 40;
constexpr std::size_t BmpHeaderBytes = BmpFileHeaderBytes + BmpInfoHeaderBytes;

void WriteLittleEndian16(
    std::array<std::uint8_t, BmpHeaderBytes>& bytes,
    const std::size_t offset,
    const std::uint16_t value) noexcept
{
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void WriteLittleEndian32(
    std::array<std::uint8_t, BmpHeaderBytes>& bytes,
    const std::size_t offset,
    const std::uint32_t value) noexcept
{
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] std::size_t ExpectedCanonicalByteCount(const CapturedFrame& frame)
{
    if (frame.width == 0 || frame.height == 0)
    {
        throw std::invalid_argument{"Captured frame dimensions must be non-zero."};
    }

    const std::uint64_t byteCount =
        static_cast<std::uint64_t>(frame.width) * static_cast<std::uint64_t>(frame.height) * Rgba8BytesPerPixel;

    if (byteCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        throw std::length_error{"Captured RGBA8 frame exceeds addressable memory."};
    }

    return static_cast<std::size_t>(byteCount);
}

void WriteBmp(const std::filesystem::path& path, const CapturedFrame& frame)
{
    const std::size_t canonicalByteCount = ExpectedCanonicalByteCount(frame);
    if (frame.rgba8Pixels.size() != canonicalByteCount)
    {
        throw std::invalid_argument{"Captured RGBA8 byte count must equal width * height * 4."};
    }

    if (path.empty())
    {
        throw std::invalid_argument{"Capture artifact path must not be empty."};
    }

    if (frame.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        frame.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw std::length_error{"BMP capture dimensions exceed signed 32-bit header limits."};
    }

    const std::uint64_t fileSize = static_cast<std::uint64_t>(BmpHeaderBytes) + canonicalByteCount;
    if (fileSize > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        throw std::length_error{"BMP capture artifact exceeds 32-bit file-size limits."};
    }

    std::array<std::uint8_t, BmpHeaderBytes> header{};
    header[0] = static_cast<std::uint8_t>('B');
    header[1] = static_cast<std::uint8_t>('M');
    WriteLittleEndian32(header, 2, static_cast<std::uint32_t>(fileSize));
    WriteLittleEndian32(header, 10, static_cast<std::uint32_t>(BmpHeaderBytes));

    WriteLittleEndian32(header, 14, static_cast<std::uint32_t>(BmpInfoHeaderBytes));
    WriteLittleEndian32(header, 18, frame.width);

    const std::int32_t topDownHeight = -static_cast<std::int32_t>(frame.height);
    WriteLittleEndian32(header, 22, static_cast<std::uint32_t>(topDownHeight));
    WriteLittleEndian16(header, 26, 1U);
    WriteLittleEndian16(header, 28, 32U);
    WriteLittleEndian32(header, 34, static_cast<std::uint32_t>(canonicalByteCount));

    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream.is_open())
    {
        throw std::runtime_error{"Failed to open capture artifact for writing: " + path.string()};
    }

    stream.write(
        reinterpret_cast<const char*>(header.data()),
        static_cast<std::streamsize>(header.size()));

    const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(Rgba8BytesPerPixel);
    std::vector<std::uint8_t> bmpRow(rowBytes);

    for (std::uint32_t y = 0; y < frame.height; ++y)
    {
        const std::uint8_t* const rgbaRow =
            frame.rgba8Pixels.data() + static_cast<std::size_t>(y) * rowBytes;

        for (std::uint32_t x = 0; x < frame.width; ++x)
        {
            const std::size_t offset = static_cast<std::size_t>(x) * static_cast<std::size_t>(Rgba8BytesPerPixel);
            bmpRow[offset] = rgbaRow[offset + 2];
            bmpRow[offset + 1] = rgbaRow[offset + 1];
            bmpRow[offset + 2] = rgbaRow[offset];
            bmpRow[offset + 3] = rgbaRow[offset + 3];
        }

        stream.write(
            reinterpret_cast<const char*>(bmpRow.data()),
            static_cast<std::streamsize>(bmpRow.size()));
    }

    if (!stream)
    {
        throw std::runtime_error{"Failed while writing capture artifact: " + path.string()};
    }
}
} // namespace

bool TryBuildCaptureReadbackLayout(
    const std::uint32_t width,
    const std::uint32_t height,
    CaptureReadbackLayout& outLayout) noexcept
{
    outLayout = CaptureReadbackLayout{};

    if (width == 0 || height == 0)
    {
        return false;
    }

    const std::uint64_t packedRowBytes = static_cast<std::uint64_t>(width) * Rgba8BytesPerPixel;
    const std::uint64_t alignedRowBytes =
        ((packedRowBytes + ReadbackRowAlignmentBytes - 1U) / ReadbackRowAlignmentBytes) * ReadbackRowAlignmentBytes;
    const std::uint64_t transferBufferBytes = alignedRowBytes * static_cast<std::uint64_t>(height);

    if (alignedRowBytes > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) ||
        transferBufferBytes > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }

    outLayout.pixelsPerRow = static_cast<std::uint32_t>(alignedRowBytes / Rgba8BytesPerPixel);
    outLayout.rowPitchBytes = static_cast<std::uint32_t>(alignedRowBytes);
    outLayout.transferBufferBytes = static_cast<std::uint32_t>(transferBufferBytes);
    return true;
}

void WriteCaptureArtifact(const CaptureRequest& request, const CapturedFrame& frame)
{
    if (request.simulationFrame != frame.simulationFrame)
    {
        throw std::invalid_argument{"Capture request frame does not match captured frame identity."};
    }

    switch (request.format)
    {
    case CaptureImageFormat::Bmp:
        WriteBmp(request.artifactPath, frame);
        return;
    }

    throw std::invalid_argument{"Unsupported capture image format."};
}
} // namespace trace2d::render
