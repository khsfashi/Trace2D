#pragma once

#include <trace2d/scene/Scene.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace trace2d::audio
{
inline constexpr std::size_t AudioClipReferenceCapacity2D = 256U;
inline constexpr float MaximumAudioPitch2D = 8.0F;
inline constexpr std::size_t AudioGroupCount2D = 4U;

enum class AudioGroup2D : std::uint8_t
{
    Master = 0,
    Music,
    Sfx,
    Ui,
    Count,
};

[[nodiscard]] std::string_view ToString(AudioGroup2D value) noexcept;

struct AudioSource2D final
{
    std::string clipReference{};
    float volume{1.0F};
    float pitch{1.0F};
    bool loop{false};
    bool autoplay{false};
    AudioGroup2D group{AudioGroup2D::Sfx};

    [[nodiscard]] bool operator==(const AudioSource2D&) const noexcept = default;
};

struct AudioComponentTypes2D final
{
    scene::ComponentTypeHandle<AudioSource2D> source{};
};

[[nodiscard]] bool ValidateAudioSource2D(const AudioSource2D& source, std::string& error);
[[nodiscard]] AudioComponentTypes2D RegisterAudio2DComponents(scene::ComponentRegistry& registry);
} // namespace trace2d::audio
