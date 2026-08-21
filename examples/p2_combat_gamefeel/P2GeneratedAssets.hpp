#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace trace2d::examples
{
inline constexpr char P2HitClipReference[] = "audio/p2_hit.wav";
inline constexpr std::uint32_t P2HitSampleRateHz = 48000U;
inline constexpr std::uint64_t P2HitFrameCount = 15360U;

inline void WriteP2U16(std::ofstream& stream, const std::uint16_t value)
{
    const std::array<char, 2U> bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
    };
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline void WriteP2U32(std::ofstream& stream, const std::uint32_t value)
{
    const std::array<char, 4U> bytes{
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
        static_cast<char>((value >> 16U) & 0xFFU),
        static_cast<char>((value >> 24U) & 0xFFU),
    };
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline void EnsureP2GeneratedHitSfx(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path output = projectRoot / P2HitClipReference;
    std::error_code error{};
    std::filesystem::create_directories(output.parent_path(), error);
    if (error) throw std::runtime_error{"P2 could not create the generated-audio directory."};

    constexpr std::uint16_t channelCount = 1U;
    constexpr std::uint16_t bitsPerSample = 16U;
    constexpr std::uint16_t blockAlign = channelCount * (bitsPerSample / 8U);
    constexpr std::uint32_t byteRate = P2HitSampleRateHz * blockAlign;
    constexpr std::uint32_t dataBytes = static_cast<std::uint32_t>(P2HitFrameCount * blockAlign);

    const std::uintmax_t expectedBytes = 44U + dataBytes;
    const std::uintmax_t existingBytes = std::filesystem::file_size(output, error);
    if (!error && existingBytes == expectedBytes) return;
    error.clear();

    std::ofstream stream{output, std::ios::binary | std::ios::trunc};
    if (!stream.is_open()) throw std::runtime_error{"P2 could not create the generated hit SFX."};

    stream.write("RIFF", 4);
    WriteP2U32(stream, 36U + dataBytes);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    WriteP2U32(stream, 16U);
    WriteP2U16(stream, 1U);
    WriteP2U16(stream, channelCount);
    WriteP2U32(stream, P2HitSampleRateHz);
    WriteP2U32(stream, byteRate);
    WriteP2U16(stream, blockAlign);
    WriteP2U16(stream, bitsPerSample);
    stream.write("data", 4);
    WriteP2U32(stream, dataBytes);

    constexpr double Pi = 3.14159265358979323846;
    constexpr double amplitude = 0.62;
    for (std::uint64_t frame = 0U; frame < P2HitFrameCount; ++frame)
    {
        const double time = static_cast<double>(frame) / static_cast<double>(P2HitSampleRateHz);
        const double envelope = std::exp(-15.0 * time);
        const double body = std::sin(2.0 * Pi * 150.0 * time);
        const double snap = std::sin(2.0 * Pi * 720.0 * time) * std::exp(-32.0 * time);
        const double mixed = std::clamp(amplitude * envelope * (body + 0.35 * snap), -1.0, 1.0);
        const auto sample = static_cast<std::int16_t>(std::lround(mixed * 32767.0));
        WriteP2U16(stream, static_cast<std::uint16_t>(sample));
    }

    stream.flush();
    if (!stream.good()) throw std::runtime_error{"P2 generated hit SFX write failed."};
}
} // namespace trace2d::examples
