#include <trace2d/audio/AudioComponents2D.hpp>

#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::audio
{
namespace
{
[[nodiscard]] bool IsAsciiAlpha(const char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool IsPortableProjectRelativeReference(const std::string_view value) noexcept
{
    if (value.empty() || value.size() >= AudioClipReferenceCapacity2D)
    {
        return false;
    }
    if (value.front() == '/' || value.front() == '\\' ||
        (value.size() >= 2U && IsAsciiAlpha(value[0]) && value[1] == ':'))
    {
        return false;
    }

    bool hasCanonicalSegment = false;
    std::size_t cursor = 0U;
    while (cursor <= value.size())
    {
        std::size_t end = cursor;
        while (end < value.size() && value[end] != '/' && value[end] != '\\')
        {
            const unsigned char character = static_cast<unsigned char>(value[end]);
            if (character < 0x20U || value[end] == ':')
            {
                return false;
            }
            ++end;
        }

        const std::string_view segment = value.substr(cursor, end - cursor);
        if (segment == "..")
        {
            return false;
        }
        if (!segment.empty() && segment != ".")
        {
            hasCanonicalSegment = true;
        }
        if (end == value.size())
        {
            break;
        }
        cursor = end + 1U;
    }
    return hasCanonicalSegment;
}

[[nodiscard]] scene::SemanticValue BooleanValue(const bool value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Boolean;
    semantic.booleanValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue FloatValue(const double value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Float;
    semantic.floatValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue ResourceValue(std::string value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::ResourceReference;
    semantic.textValue = std::move(value);
    return semantic;
}

[[nodiscard]] scene::SemanticValue EnumValue(std::string value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::EnumName;
    semantic.textValue = std::move(value);
    return semantic;
}

[[nodiscard]] bool ParseGroup(const std::string_view text, AudioGroup2D& outGroup) noexcept
{
    if (text == "master") outGroup = AudioGroup2D::Master;
    else if (text == "music") outGroup = AudioGroup2D::Music;
    else if (text == "sfx") outGroup = AudioGroup2D::Sfx;
    else if (text == "ui") outGroup = AudioGroup2D::Ui;
    else return false;
    return true;
}

[[nodiscard]] bool ReadFloat(
    const scene::SemanticValue* value,
    float& destination,
    const std::string_view field,
    std::string& error)
{
    if (value == nullptr || value->kind != scene::SemanticValueKind::Float)
    {
        error = std::string{field} + " must be float.";
        return false;
    }
    destination = static_cast<float>(value->floatValue);
    return true;
}

[[nodiscard]] bool ReadBoolean(
    const scene::SemanticValue* value,
    bool& destination,
    const std::string_view field,
    std::string& error)
{
    if (value == nullptr || value->kind != scene::SemanticValueKind::Boolean)
    {
        error = std::string{field} + " must be bool.";
        return false;
    }
    destination = value->booleanValue;
    return true;
}

[[nodiscard]] bool ParseAudioSource(
    const scene::ComponentAuthoringObject& authored,
    AudioSource2D& source,
    std::string& error)
{
    if (authored.fields.size() != 6U)
    {
        error = "trace2d.audiosource2d expects exactly 6 authored fields.";
        return false;
    }

    const scene::SemanticValue* const clip = authored.Find("clip");
    const scene::SemanticValue* const group = authored.Find("group");
    if (clip == nullptr ||
        (clip->kind != scene::SemanticValueKind::Text && clip->kind != scene::SemanticValueKind::ResourceReference))
    {
        error = "trace2d.audiosource2d.clip must be a resource reference.";
        return false;
    }
    if (group == nullptr ||
        (group->kind != scene::SemanticValueKind::Text && group->kind != scene::SemanticValueKind::EnumName) ||
        !ParseGroup(group->textValue, source.group))
    {
        error = "trace2d.audiosource2d.group must be master, music, sfx, or ui.";
        return false;
    }

    source.clipReference = clip->textValue;
    if (!ReadFloat(authored.Find("volume"), source.volume, "trace2d.audiosource2d.volume", error) ||
        !ReadFloat(authored.Find("pitch"), source.pitch, "trace2d.audiosource2d.pitch", error) ||
        !ReadBoolean(authored.Find("loop"), source.loop, "trace2d.audiosource2d.loop", error) ||
        !ReadBoolean(authored.Find("autoplay"), source.autoplay, "trace2d.audiosource2d.autoplay", error))
    {
        return false;
    }
    return ValidateAudioSource2D(source, error);
}

[[nodiscard]] scene::ComponentAuthoringObject SerializeAudioSource(const AudioSource2D& source)
{
    scene::ComponentAuthoringObject authored{};
    authored.fields.reserve(6U);
    authored.fields.push_back({"clip", ResourceValue(source.clipReference)});
    authored.fields.push_back({"volume", FloatValue(source.volume)});
    authored.fields.push_back({"pitch", FloatValue(source.pitch)});
    authored.fields.push_back({"loop", BooleanValue(source.loop)});
    authored.fields.push_back({"autoplay", BooleanValue(source.autoplay)});
    authored.fields.push_back({"group", EnumValue(std::string{ToString(source.group)})});
    return authored;
}

[[nodiscard]] std::vector<scene::ComponentInspectionField> InspectAudioSource(const AudioSource2D& source)
{
    const scene::ComponentAuthoringObject authored = SerializeAudioSource(source);
    std::vector<scene::ComponentInspectionField> fields{};
    fields.reserve(authored.fields.size());
    for (const scene::ComponentAuthoringField& field : authored.fields)
    {
        fields.push_back({field.name, field.value});
    }
    return fields;
}
} // namespace

std::string_view ToString(const AudioGroup2D value) noexcept
{
    switch (value)
    {
    case AudioGroup2D::Master:
        return "master";
    case AudioGroup2D::Music:
        return "music";
    case AudioGroup2D::Sfx:
        return "sfx";
    case AudioGroup2D::Ui:
        return "ui";
    case AudioGroup2D::Count:
        break;
    }
    return "unknown";
}

bool ValidateAudioSource2D(const AudioSource2D& source, std::string& error)
{
    if (!IsPortableProjectRelativeReference(source.clipReference))
    {
        error = "trace2d.audiosource2d.clip must be a bounded portable project-relative resource reference.";
        return false;
    }
    if (!std::isfinite(source.volume) || source.volume < 0.0F || source.volume > 1.0F)
    {
        error = "trace2d.audiosource2d.volume must be finite and in [0,1].";
        return false;
    }
    if (!std::isfinite(source.pitch) || source.pitch <= 0.0F || source.pitch > MaximumAudioPitch2D)
    {
        error = "trace2d.audiosource2d.pitch must be finite and in (0,8].";
        return false;
    }
    if (source.group >= AudioGroup2D::Count)
    {
        error = "trace2d.audiosource2d.group is invalid.";
        return false;
    }
    return true;
}

AudioComponentTypes2D RegisterAudio2DComponents(scene::ComponentRegistry& registry)
{
    scene::ComponentRegistration<AudioSource2D> registration{};
    registration.typeId = "trace2d.audiosource2d";
    registration.schemaVersion = 1U;
    registration.componentClass = scene::ComponentClass::Authored;
    registration.parseAuthored = ParseAudioSource;
    registration.validate = ValidateAudioSource2D;
    registration.serializeAuthored = SerializeAudioSource;
    registration.inspect = InspectAudioSource;
    return AudioComponentTypes2D{registry.Register(std::move(registration))};
}
} // namespace trace2d::audio
