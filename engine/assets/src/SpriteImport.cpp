#include <trace2d/assets/SpriteImport.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace trace2d::assets
{
namespace
{
using Json = nlohmann::ordered_json;
constexpr std::uint64_t NanosecondsPerMillisecond = 1'000'000ULL;

void AddDiagnostic(
    SpriteImportResult& result,
    const SpriteImportErrorCode code,
    std::string path,
    std::string message)
{
    result.diagnostics.push_back(SpriteImportDiagnostic{
        .code = code,
        .path = std::move(path),
        .message = std::move(message),
    });
}

void ClearPartial(SpriteImportResult& result)
{
    result.asset = {};
    result.frames.clear();
    result.tags.clear();
}

bool ValidateBytes(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::span<const std::uint8_t> rgba8,
    const std::string& path,
    SpriteImportResult& result)
{
    if (width == 0U || height == 0U)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDimensions,
            path,
            "Decoded image dimensions must be positive.");
        return false;
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels > std::numeric_limits<std::size_t>::max() / 4U)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::SizeOverflow,
            path,
            "Decoded image RGBA8 byte count overflows size_t.");
        return false;
    }
    const std::size_t expected = static_cast<std::size_t>(pixels) * 4U;
    if (rgba8.size() != expected)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidByteCount,
            path,
            "Decoded image must contain exactly width*height*4 RGBA8 bytes.");
        return false;
    }
    return true;
}

bool RectFits(
    const SpritePixelRect rect,
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    return rect.width > 0U && rect.height > 0U &&
        static_cast<std::uint64_t>(rect.x) + rect.width <= width &&
        static_cast<std::uint64_t>(rect.y) + rect.height <= height;
}

bool TrimFits(
    const SpritePixelSize source,
    const SpritePixelOffset offset,
    const SpritePixelSize trim) noexcept
{
    return source.width > 0U && source.height > 0U &&
        trim.width > 0U && trim.height > 0U &&
        static_cast<std::uint64_t>(offset.x) + trim.width <= source.width &&
        static_cast<std::uint64_t>(offset.y) + trim.height <= source.height;
}

bool ValidateCommon(
    const std::string_view assetId,
    const std::string_view pageId,
    const std::string_view textureReference,
    SpriteImportResult& result)
{
    bool valid = true;
    if (assetId.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::EmptyAssetId,
            "$.asset",
            "Canonical Sprite asset ID must not be empty.");
        valid = false;
    }
    if (pageId.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::EmptyPageId,
            "$.page_id",
            "Page ID must not be empty.");
        valid = false;
    }
    if (textureReference.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::EmptyTextureReference,
            "$.texture_reference",
            "Texture reference must not be empty.");
        valid = false;
    }
    return valid;
}

SpriteAtlasPage MakePage(
    const std::string_view pageId,
    const std::string_view textureReference,
    const SpritePixelSize size,
    const SpriteColorSpace colorSpace)
{
    return SpriteAtlasPage{
        .id = std::string{pageId},
        .textureReference = std::string{textureReference},
        .size = size,
        .colorSpace = colorSpace,
        .alphaMode = SpriteAlphaMode::Straight,
    };
}

SpriteRegion MakeRegion(
    const std::string_view regionId,
    const std::string_view pageId,
    const SpritePixelSize sourceSize,
    const SpritePixelOffset trimOffset,
    const SpritePixelSize trimSize,
    const SpritePixelRect packedRect,
    const SpriteRationalPivot pivot,
    const SpritePackedRotation rotation)
{
    return SpriteRegion{
        .id = std::string{regionId},
        .pageId = std::string{pageId},
        .sourceSize = sourceSize,
        .trimOffset = trimOffset,
        .trimSize = trimSize,
        .packedRect = packedRect,
        .pivot = pivot,
        .packedRotation = rotation,
        .border = {},
    };
}

bool Canonicalize(SpriteImportResult& result)
{
    const std::string serialized = SaveSpriteAssetToml(result.asset);
    SpriteAssetLoadResult canonical =
        ParseSpriteAssetToml(serialized, result.asset.id, "SPP3 import");
    if (!canonical.Succeeded())
    {
        for (const SpriteAssetDiagnostic& diagnostic : canonical.diagnostics)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::CanonicalValidationFailed,
                "$.canonical." + diagnostic.path,
                std::string{ToString(diagnostic.code)} + ": " + diagnostic.message);
        }
        return false;
    }
    result.asset = *canonical.asset;
    return true;
}

bool AddRegion(
    SpriteImportResult& result,
    std::unordered_set<std::string>& regionIds,
    SpriteRegion region,
    const std::optional<std::int64_t> durationNanoseconds,
    const std::string& path)
{
    if (region.id.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::EmptyRegionId,
            path,
            "Region ID must not be empty.");
        return false;
    }
    if (!regionIds.insert(region.id).second)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::DuplicateRegionId,
            path,
            "Region IDs must be unique.");
        return false;
    }
    result.frames.push_back(SpriteImportedFrame{
        .regionId = region.id,
        .durationNanoseconds = durationNanoseconds,
    });
    result.asset.regions.push_back(std::move(region));
    return true;
}

bool JsonUnsigned(const Json& value, std::uint64_t& output)
{
    if (value.is_number_unsigned())
    {
        output = value.get<std::uint64_t>();
        return true;
    }
    if (value.is_number_integer())
    {
        const std::int64_t signedValue = value.get<std::int64_t>();
        if (signedValue >= 0)
        {
            output = static_cast<std::uint64_t>(signedValue);
            return true;
        }
    }
    return false;
}

bool ReadU32(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    std::uint32_t& output,
    SpriteImportResult& result)
{
    const auto iterator = object.find(std::string{key});
    if (iterator == object.end())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::MissingField,
            path + "." + std::string{key},
            "Required integer field is missing.");
        return false;
    }
    std::uint64_t value = 0U;
    if (!JsonUnsigned(*iterator, value) ||
        value > std::numeric_limits<std::uint32_t>::max())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "Expected a non-negative integer that fits uint32.");
        return false;
    }
    output = static_cast<std::uint32_t>(value);
    return true;
}

bool ReadBool(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    bool& output,
    SpriteImportResult& result)
{
    const auto iterator = object.find(std::string{key});
    if (iterator == object.end())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::MissingField,
            path + "." + std::string{key},
            "Required boolean field is missing.");
        return false;
    }
    if (!iterator->is_boolean())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "Expected a boolean.");
        return false;
    }
    output = iterator->get<bool>();
    return true;
}

bool ReadString(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    std::string& output,
    SpriteImportResult& result)
{
    const auto iterator = object.find(std::string{key});
    if (iterator == object.end())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::MissingField,
            path + "." + std::string{key},
            "Required string field is missing.");
        return false;
    }
    if (!iterator->is_string())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "Expected a string.");
        return false;
    }
    output = iterator->get<std::string>();
    if (output.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "String field must not be empty.");
        return false;
    }
    return true;
}

bool ReadRect(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    SpritePixelRect& rect,
    SpriteImportResult& result)
{
    const auto iterator = object.find(std::string{key});
    if (iterator == object.end() || !iterator->is_object())
    {
        AddDiagnostic(
            result,
            iterator == object.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "Expected an object with x/y/w/h.");
        return false;
    }
    bool valid = true;
    valid = ReadU32(*iterator, "x", path + "." + std::string{key}, rect.x, result) && valid;
    valid = ReadU32(*iterator, "y", path + "." + std::string{key}, rect.y, result) && valid;
    valid = ReadU32(*iterator, "w", path + "." + std::string{key}, rect.width, result) && valid;
    valid = ReadU32(*iterator, "h", path + "." + std::string{key}, rect.height, result) && valid;
    return valid;
}

bool ReadSize(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    SpritePixelSize& size,
    SpriteImportResult& result)
{
    const auto iterator = object.find(std::string{key});
    if (iterator == object.end() || !iterator->is_object())
    {
        AddDiagnostic(
            result,
            iterator == object.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "Expected an object with w/h.");
        return false;
    }
    bool valid = true;
    valid = ReadU32(*iterator, "w", path + "." + std::string{key}, size.width, result) && valid;
    valid = ReadU32(*iterator, "h", path + "." + std::string{key}, size.height, result) && valid;
    return valid;
}

std::optional<SpriteImportAnimationDirection> ParseDirection(const std::string_view value)
{
    if (value == "forward")
    {
        return SpriteImportAnimationDirection::Forward;
    }
    if (value == "reverse")
    {
        return SpriteImportAnimationDirection::Reverse;
    }
    if (value == "pingpong")
    {
        return SpriteImportAnimationDirection::PingPong;
    }
    if (value == "pingpong_reverse")
    {
        return SpriteImportAnimationDirection::PingPongReverse;
    }
    return std::nullopt;
}

bool ConvertAsepriteFrame(
    const Json& frame,
    const std::string_view regionId,
    const std::string& path,
    const SpriteImportDecodedImageView& sheet,
    const SpriteAsepriteSheetImportOptions& options,
    SpriteImportResult& result,
    std::unordered_set<std::string>& regionIds)
{
    if (!frame.is_object())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path,
            "Aseprite frame entry must be an object.");
        return false;
    }

    SpritePixelRect packed{};
    SpritePixelRect sourceRect{};
    SpritePixelSize sourceSize{};
    bool rotated = false;
    bool trimmed = false;
    std::uint32_t durationMs = 0U;
    bool valid = true;
    valid = ReadRect(frame, "frame", path, packed, result) && valid;
    valid = ReadRect(frame, "spriteSourceSize", path, sourceRect, result) && valid;
    valid = ReadSize(frame, "sourceSize", path, sourceSize, result) && valid;
    valid = ReadBool(frame, "rotated", path, rotated, result) && valid;
    valid = ReadBool(frame, "trimmed", path, trimmed, result) && valid;
    valid = ReadU32(frame, "duration", path, durationMs, result) && valid;
    if (!valid)
    {
        return false;
    }

    const SpritePixelOffset trimOffset{sourceRect.x, sourceRect.y};
    const SpritePixelSize trimSize{sourceRect.width, sourceRect.height};
    if (!RectFits(packed, sheet.width, sheet.height))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::RectangleOutOfBounds,
            path + ".frame",
            "Frame rectangle must be non-empty and inside the decoded sheet.");
        valid = false;
    }
    if (!TrimFits(sourceSize, trimOffset, trimSize))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidRectangle,
            path + ".spriteSourceSize",
            "Trim rectangle must be non-empty and inside sourceSize.");
        valid = false;
    }
    if (!trimmed &&
        (trimOffset.x != 0U || trimOffset.y != 0U ||
         trimSize.width != sourceSize.width || trimSize.height != sourceSize.height))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidRectangle,
            path + ".trimmed",
            "trimmed=false requires spriteSourceSize to cover sourceSize.");
        valid = false;
    }

    SpritePackedRotation rotation = SpritePackedRotation::None;
    if (rotated)
    {
        if (options.rotatedFramePolicy !=
            SpriteAsepriteRotatedFramePolicy::InterpretAsCw90)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::UnsupportedRotation,
                path + ".rotated",
                "rotated=true requires explicit InterpretAsCw90 policy.");
            valid = false;
        }
        else
        {
            rotation = SpritePackedRotation::Cw90;
            if (packed.width != trimSize.height || packed.height != trimSize.width)
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::InvalidRectangle,
                    path + ".frame",
                    "cw90 packed extent must equal swapped trim extent.");
                valid = false;
            }
        }
    }
    else if (packed.width != trimSize.width || packed.height != trimSize.height)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidRectangle,
            path + ".frame",
            "Unrotated packed extent must equal trim extent.");
        valid = false;
    }

    if (durationMs == 0U ||
        static_cast<std::uint64_t>(durationMs) >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) /
                NanosecondsPerMillisecond)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDuration,
            path + ".duration",
            "Duration must be positive and fit exact int64 nanoseconds.");
        valid = false;
    }
    if (!valid)
    {
        return false;
    }

    const std::int64_t durationNs = static_cast<std::int64_t>(
        static_cast<std::uint64_t>(durationMs) * NanosecondsPerMillisecond);
    return AddRegion(
        result,
        regionIds,
        MakeRegion(
            regionId,
            options.pageId,
            sourceSize,
            trimOffset,
            trimSize,
            packed,
            options.defaultPivot,
            rotation),
        durationNs,
        path);
}

bool ParseTags(const Json& meta, SpriteImportResult& result)
{
    const auto iterator = meta.find("frameTags");
    if (iterator == meta.end())
    {
        return true;
    }
    if (!iterator->is_array())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            "$.meta.frameTags",
            "frameTags must be an array.");
        return false;
    }

    std::unordered_set<std::string> names{};
    bool valid = true;
    for (std::size_t index = 0U; index < iterator->size(); ++index)
    {
        const Json& tag = (*iterator)[index];
        const std::string path = "$.meta.frameTags[" + std::to_string(index) + "]";
        if (!tag.is_object())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path,
                "Frame tag must be an object.");
            valid = false;
            continue;
        }

        std::string name{};
        std::uint32_t from = 0U;
        std::uint32_t to = 0U;
        bool tagValid = true;
        tagValid = ReadString(tag, "name", path, name, result) && tagValid;
        tagValid = ReadU32(tag, "from", path, from, result) && tagValid;
        tagValid = ReadU32(tag, "to", path, to, result) && tagValid;
        if (!tagValid)
        {
            valid = false;
            continue;
        }
        if (!names.insert(name).second)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::DuplicateTag,
                path + ".name",
                "Frame tag names must be unique.");
            valid = false;
            continue;
        }
        if (to < from ||
            static_cast<std::uint64_t>(to) >=
                static_cast<std::uint64_t>(result.frames.size()))
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidTagRange,
                path,
                "Frame tag range must be ordered and inside imported frames.");
            valid = false;
            continue;
        }

        SpriteImportAnimationDirection direction =
            SpriteImportAnimationDirection::Forward;
        const auto directionIterator = tag.find("direction");
        if (directionIterator != tag.end())
        {
            if (!directionIterator->is_string())
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::UnsupportedTagDirection,
                    path + ".direction",
                    "Frame tag direction must be a supported string.");
                valid = false;
                continue;
            }
            const std::optional<SpriteImportAnimationDirection> parsed =
                ParseDirection(directionIterator->get<std::string>());
            if (!parsed.has_value())
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::UnsupportedTagDirection,
                    path + ".direction",
                    "Supported directions: forward, reverse, pingpong, pingpong_reverse.");
                valid = false;
                continue;
            }
            direction = *parsed;
        }

        result.tags.push_back(SpriteImportedTag{
            .name = std::move(name),
            .firstFrame = from,
            .lastFrame = to,
            .direction = direction,
        });
    }
    return valid;
}

bool AddGenericRegion(
    const SpriteGenericRegionView& input,
    const SpriteImportDecodedImageView& sheet,
    const SpriteGenericSheetImportSpec& spec,
    const SpriteGenericSheetImportOptions& options,
    const std::string& path,
    SpriteImportResult& result,
    std::unordered_set<std::string>& regionIds)
{
    if (!RectFits(input.packedRect, sheet.width, sheet.height))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::RectangleOutOfBounds,
            path + ".packed_rect",
            "Packed rectangle must be non-empty and inside the decoded sheet.");
        return false;
    }

    const SpritePixelSize packedLogical =
        input.packedRotation == SpritePackedRotation::Cw90
        ? SpritePixelSize{input.packedRect.height, input.packedRect.width}
        : SpritePixelSize{input.packedRect.width, input.packedRect.height};
    const SpritePixelSize trimSize = input.trimSize.value_or(packedLogical);
    const SpritePixelOffset trimOffset = input.trimOffset.value_or(SpritePixelOffset{});
    const SpritePixelSize sourceSize = input.sourceSize.value_or(trimSize);
    if (!TrimFits(sourceSize, trimOffset, trimSize))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidRectangle,
            path,
            "Source/trim metadata must describe a non-empty trim inside sourceSize.");
        return false;
    }

    if (input.packedRotation == SpritePackedRotation::None &&
        (input.packedRect.width != trimSize.width ||
         input.packedRect.height != trimSize.height))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidRectangle,
            path,
            "Unrotated packed extent must equal trimSize.");
        return false;
    }
    if (input.packedRotation == SpritePackedRotation::Cw90 &&
        (input.packedRect.width != trimSize.height ||
         input.packedRect.height != trimSize.width))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidRectangle,
            path,
            "cw90 packed extent must equal swapped trimSize.");
        return false;
    }

    return AddRegion(
        result,
        regionIds,
        MakeRegion(
            input.id,
            options.pageId,
            sourceSize,
            trimOffset,
            trimSize,
            input.packedRect,
            input.pivot.value_or(spec.defaultPivot),
            input.packedRotation),
        std::nullopt,
        path + ".id");
}

void WriteAssetJson(Json& root, const SpriteAsset& asset)
{
    Json value = Json::object();
    value["id"] = asset.id;
    value["schema_version"] = asset.schemaVersion;
    value["sampling"] = std::string{ToString(asset.sampling)};

    Json pages = Json::array();
    for (const SpriteAtlasPage& page : asset.pages)
    {
        Json item = Json::object();
        item["id"] = page.id;
        item["texture_reference"] = page.textureReference;
        item["size"] = Json::array({page.size.width, page.size.height});
        item["color_space"] = std::string{ToString(page.colorSpace)};
        item["alpha_mode"] = std::string{ToString(page.alphaMode)};
        pages.push_back(std::move(item));
    }
    value["pages"] = std::move(pages);

    Json regions = Json::array();
    for (const SpriteRegion& region : asset.regions)
    {
        Json item = Json::object();
        item["id"] = region.id;
        item["page_id"] = region.pageId;
        item["source_size"] =
            Json::array({region.sourceSize.width, region.sourceSize.height});
        item["trim_offset"] =
            Json::array({region.trimOffset.x, region.trimOffset.y});
        item["trim_size"] =
            Json::array({region.trimSize.width, region.trimSize.height});
        item["packed_rect"] = Json::array(
            {region.packedRect.x, region.packedRect.y,
             region.packedRect.width, region.packedRect.height});
        item["pivot"] = Json::array(
            {region.pivot.xNumerator, region.pivot.yNumerator,
             region.pivot.denominator});
        item["packed_rotation"] = std::string{ToString(region.packedRotation)};
        item["border"] = Json::array(
            {region.border.left, region.border.top,
             region.border.right, region.border.bottom});
        regions.push_back(std::move(item));
    }
    value["regions"] = std::move(regions);
    root["asset"] = std::move(value);
}
} // namespace

std::string_view ToString(const SpriteImportSourceKind value) noexcept
{
    switch (value)
    {
    case SpriteImportSourceKind::AsepriteSheetJson: return "aseprite_sheet_json";
    case SpriteImportSourceKind::GenericSheet: return "generic_sheet";
    case SpriteImportSourceKind::LooseFrames: return "loose_frames";
    }
    return "unknown";
}

std::string_view ToString(const SpriteImportAnimationDirection value) noexcept
{
    switch (value)
    {
    case SpriteImportAnimationDirection::Forward: return "forward";
    case SpriteImportAnimationDirection::Reverse: return "reverse";
    case SpriteImportAnimationDirection::PingPong: return "pingpong";
    case SpriteImportAnimationDirection::PingPongReverse: return "pingpong_reverse";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAsepriteRotatedFramePolicy value) noexcept
{
    switch (value)
    {
    case SpriteAsepriteRotatedFramePolicy::Reject: return "reject";
    case SpriteAsepriteRotatedFramePolicy::InterpretAsCw90: return "interpret_as_cw90";
    }
    return "unknown";
}

std::string_view ToString(const SpriteGenericSheetMode value) noexcept
{
    switch (value)
    {
    case SpriteGenericSheetMode::ExplicitRegions: return "explicit_regions";
    case SpriteGenericSheetMode::UniformGrid: return "uniform_grid";
    }
    return "unknown";
}

std::string_view ToString(const SpriteImportErrorCode value) noexcept
{
    switch (value)
    {
    case SpriteImportErrorCode::EmptyAssetId: return "empty_asset_id";
    case SpriteImportErrorCode::EmptyPageId: return "empty_page_id";
    case SpriteImportErrorCode::EmptyTextureReference: return "empty_texture_reference";
    case SpriteImportErrorCode::InvalidDimensions: return "invalid_dimensions";
    case SpriteImportErrorCode::InvalidByteCount: return "invalid_byte_count";
    case SpriteImportErrorCode::SizeOverflow: return "size_overflow";
    case SpriteImportErrorCode::JsonParseError: return "json_parse_error";
    case SpriteImportErrorCode::UnsupportedManifest: return "unsupported_manifest";
    case SpriteImportErrorCode::MissingField: return "missing_field";
    case SpriteImportErrorCode::InvalidField: return "invalid_field";
    case SpriteImportErrorCode::EmptyRegionId: return "empty_region_id";
    case SpriteImportErrorCode::DuplicateRegionId: return "duplicate_region_id";
    case SpriteImportErrorCode::InvalidRectangle: return "invalid_rectangle";
    case SpriteImportErrorCode::RectangleOutOfBounds: return "rectangle_out_of_bounds";
    case SpriteImportErrorCode::UnsupportedRotation: return "unsupported_rotation";
    case SpriteImportErrorCode::InvalidDuration: return "invalid_duration";
    case SpriteImportErrorCode::DuplicateTag: return "duplicate_tag";
    case SpriteImportErrorCode::InvalidTagRange: return "invalid_tag_range";
    case SpriteImportErrorCode::UnsupportedTagDirection: return "unsupported_tag_direction";
    case SpriteImportErrorCode::InvalidGenericSpec: return "invalid_generic_spec";
    case SpriteImportErrorCode::ExpectedFrameCountMismatch: return "expected_frame_count_mismatch";
    case SpriteImportErrorCode::DuplicatePageId: return "duplicate_page_id";
    case SpriteImportErrorCode::CanonicalValidationFailed: return "canonical_validation_failed";
    }
    return "unknown";
}

SpriteImportResult ImportAsepriteSpriteSheetJson(
    const std::string_view jsonText,
    const SpriteImportDecodedImageView& sheet,
    const SpriteAsepriteSheetImportOptions& options)
{
    SpriteImportResult result{};
    result.sourceKind = SpriteImportSourceKind::AsepriteSheetJson;
    ValidateCommon(
        options.canonicalAssetId,
        options.pageId,
        options.textureReference,
        result);
    ValidateBytes(sheet.width, sheet.height, sheet.rgba8, "$.decoded_sheet", result);
    if (!result.diagnostics.empty())
    {
        return result;
    }

    Json root{};
    try
    {
        root = Json::parse(jsonText.begin(), jsonText.end());
    }
    catch (const Json::parse_error& error)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::JsonParseError,
            "$",
            std::string{"Aseprite JSON parse failed: "} + error.what());
        return result;
    }
    if (!root.is_object())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$",
            "Aseprite manifest root must be an object.");
        return result;
    }

    const auto framesIterator = root.find("frames");
    const auto metaIterator = root.find("meta");
    if (framesIterator == root.end())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::MissingField,
            "$.frames",
            "Aseprite manifest requires frames.");
    }
    if (metaIterator == root.end() || !metaIterator->is_object())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::MissingField,
            "$.meta",
            "Aseprite manifest requires meta object.");
    }
    if (!result.diagnostics.empty())
    {
        return result;
    }

    SpritePixelSize metaSize{};
    std::string metaImage{};
    bool metaValid = true;
    metaValid = ReadSize(*metaIterator, "size", "$.meta", metaSize, result) && metaValid;
    metaValid = ReadString(*metaIterator, "image", "$.meta", metaImage, result) && metaValid;
    if (!metaValid)
    {
        return result;
    }
    if (metaSize.width != sheet.width || metaSize.height != sheet.height)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDimensions,
            "$.meta.size",
            "meta.size must exactly match the supplied decoded sheet.");
        return result;
    }
    if (sheet.id.empty() || metaImage != sheet.id)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            "$.meta.image",
            "meta.image must exactly match the supplied decoded sheet ID.");
        return result;
    }

    const auto scaleIterator = metaIterator->find("scale");
    if (scaleIterator == metaIterator->end() || !scaleIterator->is_string() ||
        scaleIterator->get<std::string>() != "1")
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$.meta.scale",
            "SPP3 requires unscaled Aseprite output with meta.scale equal to \"1\".");
        return result;
    }
    const auto formatIterator = metaIterator->find("format");
    if (formatIterator != metaIterator->end() &&
        (!formatIterator->is_string() ||
         formatIterator->get<std::string>() != "RGBA8888"))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$.meta.format",
            "SPP3 supports RGBA8888 Aseprite manifests.");
        return result;
    }

    result.asset = SpriteAsset{
        .id = std::string{options.canonicalAssetId},
        .schemaVersion = 1U,
        .sampling = options.sampling,
        .pages = {MakePage(
            options.pageId,
            options.textureReference,
            SpritePixelSize{sheet.width, sheet.height},
            options.colorSpace)},
        .regions = {},
    };

    std::unordered_set<std::string> regionIds{};
    bool valid = true;
    if (framesIterator->is_array())
    {
        if (framesIterator->empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::UnsupportedManifest,
                "$.frames",
                "frames array must not be empty.");
            valid = false;
        }
        for (std::size_t index = 0U; index < framesIterator->size(); ++index)
        {
            const Json& frame = (*framesIterator)[index];
            const std::string path = "$.frames[" + std::to_string(index) + "]";
            std::string regionId{};
            if (!frame.is_object() ||
                !ReadString(frame, "filename", path, regionId, result))
            {
                valid = false;
                continue;
            }
            valid = ConvertAsepriteFrame(
                frame, regionId, path, sheet, options, result, regionIds) && valid;
        }
    }
    else if (framesIterator->is_object())
    {
        if (framesIterator->empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::UnsupportedManifest,
                "$.frames",
                "frames object must not be empty.");
            valid = false;
        }
        std::size_t index = 0U;
        for (const auto& item : framesIterator->items())
        {
            valid = ConvertAsepriteFrame(
                item.value(),
                item.key(),
                "$.frames[" + std::to_string(index) + "]",
                sheet,
                options,
                result,
                regionIds) && valid;
            ++index;
        }
    }
    else
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$.frames",
            "frames must use JSON array or object form.");
        valid = false;
    }

    if (valid)
    {
        valid = ParseTags(*metaIterator, result);
    }
    if (valid)
    {
        valid = Canonicalize(result);
    }
    if (!valid || !result.diagnostics.empty())
    {
        ClearPartial(result);
    }
    return result;
}

SpriteImportResult ImportGenericSpriteSheet(
    const SpriteImportDecodedImageView& sheet,
    const SpriteGenericSheetImportSpec& spec,
    const SpriteGenericSheetImportOptions& options)
{
    SpriteImportResult result{};
    result.sourceKind = SpriteImportSourceKind::GenericSheet;
    ValidateCommon(
        options.canonicalAssetId,
        options.pageId,
        options.textureReference,
        result);
    ValidateBytes(sheet.width, sheet.height, sheet.rgba8, "$.decoded_sheet", result);
    if (spec.expectedFrameCount == 0U)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidGenericSpec,
            "$.expected_frame_count",
            "expectedFrameCount must be positive.");
    }
    if (!result.diagnostics.empty())
    {
        return result;
    }

    result.asset = SpriteAsset{
        .id = std::string{options.canonicalAssetId},
        .schemaVersion = 1U,
        .sampling = options.sampling,
        .pages = {MakePage(
            options.pageId,
            options.textureReference,
            SpritePixelSize{sheet.width, sheet.height},
            options.colorSpace)},
        .regions = {},
    };

    std::unordered_set<std::string> regionIds{};
    bool valid = true;
    if (spec.mode == SpriteGenericSheetMode::ExplicitRegions)
    {
        if (spec.explicitRegions.size() != spec.expectedFrameCount)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::ExpectedFrameCountMismatch,
                "$.explicit_regions",
                "explicitRegions size must equal expectedFrameCount.");
            valid = false;
        }
        else
        {
            for (std::size_t index = 0U; index < spec.explicitRegions.size(); ++index)
            {
                valid = AddGenericRegion(
                    spec.explicitRegions[index],
                    sheet,
                    spec,
                    options,
                    "$.explicit_regions[" + std::to_string(index) + "]",
                    result,
                    regionIds) && valid;
            }
        }
    }
    else if (spec.mode == SpriteGenericSheetMode::UniformGrid)
    {
        const SpriteExtractionGridSpec& grid = spec.grid;
        const std::uint64_t count =
            static_cast<std::uint64_t>(grid.columns) * grid.rows;
        if (grid.cellWidth == 0U || grid.cellHeight == 0U ||
            grid.columns == 0U || grid.rows == 0U)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidGenericSpec,
                "$.grid",
                "Grid cell size, columns, and rows must be positive.");
            valid = false;
        }
        else if (count > std::numeric_limits<std::uint32_t>::max() ||
            count != spec.expectedFrameCount ||
            count != spec.gridRegionIds.size())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::ExpectedFrameCountMismatch,
                "$.grid",
                "rows*columns, expectedFrameCount, and region-ID count must match.");
            valid = false;
        }
        else
        {
            for (std::uint64_t sequence = 0U; sequence < count; ++sequence)
            {
                const std::uint32_t index = static_cast<std::uint32_t>(sequence);
                std::uint32_t row = 0U;
                std::uint32_t column = 0U;
                if (grid.order == SpriteExtractionOrder::RowMajor)
                {
                    row = index / grid.columns;
                    column = index % grid.columns;
                }
                else if (grid.order == SpriteExtractionOrder::ColumnMajor)
                {
                    column = index / grid.rows;
                    row = index % grid.rows;
                }
                else
                {
                    AddDiagnostic(
                        result,
                        SpriteImportErrorCode::InvalidGenericSpec,
                        "$.grid.order",
                        "Unsupported grid order.");
                    valid = false;
                    break;
                }

                const std::uint64_t x =
                    static_cast<std::uint64_t>(grid.originX) +
                    static_cast<std::uint64_t>(column) *
                        (static_cast<std::uint64_t>(grid.cellWidth) + grid.spacingX);
                const std::uint64_t y =
                    static_cast<std::uint64_t>(grid.originY) +
                    static_cast<std::uint64_t>(row) *
                        (static_cast<std::uint64_t>(grid.cellHeight) + grid.spacingY);
                if (x > std::numeric_limits<std::uint32_t>::max() ||
                    y > std::numeric_limits<std::uint32_t>::max())
                {
                    AddDiagnostic(
                        result,
                        SpriteImportErrorCode::SizeOverflow,
                        "$.grid[" + std::to_string(index) + "]",
                        "Grid rectangle origin overflows uint32.");
                    valid = false;
                    continue;
                }

                const SpriteGenericRegionView region{
                    .id = spec.gridRegionIds[index],
                    .packedRect = SpritePixelRect{
                        static_cast<std::uint32_t>(x),
                        static_cast<std::uint32_t>(y),
                        grid.cellWidth,
                        grid.cellHeight},
                    .sourceSize = std::nullopt,
                    .trimOffset = std::nullopt,
                    .trimSize = std::nullopt,
                    .pivot = std::optional<SpriteRationalPivot>{spec.defaultPivot},
                    .packedRotation = SpritePackedRotation::None,
                };
                valid = AddGenericRegion(
                    region,
                    sheet,
                    spec,
                    options,
                    "$.grid[" + std::to_string(index) + "]",
                    result,
                    regionIds) && valid;
            }
        }
    }
    else
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidGenericSpec,
            "$.mode",
            "Unsupported generic sheet mode.");
        valid = false;
    }

    if (valid)
    {
        valid = Canonicalize(result);
    }
    if (!valid || !result.diagnostics.empty())
    {
        ClearPartial(result);
    }
    return result;
}

SpriteImportResult ImportLooseSpriteFrames(
    const std::span<const SpriteLooseFrameView> frames,
    const SpriteLooseFrameImportOptions& options)
{
    SpriteImportResult result{};
    result.sourceKind = SpriteImportSourceKind::LooseFrames;
    if (options.canonicalAssetId.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::EmptyAssetId,
            "$.asset",
            "Canonical Sprite asset ID must not be empty.");
        return result;
    }
    if (frames.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::ExpectedFrameCountMismatch,
            "$.frames",
            "Loose-frame import requires at least one frame.");
        return result;
    }

    result.asset = SpriteAsset{
        .id = std::string{options.canonicalAssetId},
        .schemaVersion = 1U,
        .sampling = options.sampling,
        .pages = {},
        .regions = {},
    };
    result.asset.pages.reserve(frames.size());
    result.asset.regions.reserve(frames.size());
    result.frames.reserve(frames.size());

    std::unordered_set<std::string> pageIds{};
    std::unordered_set<std::string> regionIds{};
    bool valid = true;
    for (std::size_t index = 0U; index < frames.size(); ++index)
    {
        const SpriteLooseFrameView& frame = frames[index];
        const std::string path = "$.frames[" + std::to_string(index) + "]";
        bool frameValid = true;
        if (frame.pageId.empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::EmptyPageId,
                path + ".page_id",
                "Loose-frame page ID must not be empty.");
            frameValid = false;
        }
        else if (!pageIds.insert(std::string{frame.pageId}).second)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::DuplicatePageId,
                path + ".page_id",
                "Loose-frame page IDs must be unique.");
            frameValid = false;
        }
        if (frame.textureReference.empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::EmptyTextureReference,
                path + ".texture_reference",
                "Loose-frame texture reference must not be empty.");
            frameValid = false;
        }
        frameValid = ValidateBytes(
            frame.width, frame.height, frame.rgba8, path, result) && frameValid;
        if (!frameValid)
        {
            valid = false;
            continue;
        }

        result.asset.pages.push_back(MakePage(
            frame.pageId,
            frame.textureReference,
            SpritePixelSize{frame.width, frame.height},
            options.colorSpace));
        valid = AddRegion(
            result,
            regionIds,
            MakeRegion(
                frame.regionId,
                frame.pageId,
                SpritePixelSize{frame.width, frame.height},
                SpritePixelOffset{},
                SpritePixelSize{frame.width, frame.height},
                SpritePixelRect{0U, 0U, frame.width, frame.height},
                frame.pivot.value_or(options.defaultPivot),
                SpritePackedRotation::None),
            std::nullopt,
            path + ".region_id") && valid;
    }

    if (valid)
    {
        valid = Canonicalize(result);
    }
    if (!valid || !result.diagnostics.empty())
    {
        ClearPartial(result);
    }
    return result;
}

std::string SerializeSpriteImportResultJson(const SpriteImportResult& result)
{
    Json root = Json::object();
    root["schema"] = "trace2d.sprite-import.v1";
    root["source_kind"] = std::string{ToString(result.sourceKind)};
    root["succeeded"] = result.Succeeded();
    WriteAssetJson(root, result.asset);

    Json frames = Json::array();
    for (const SpriteImportedFrame& frame : result.frames)
    {
        Json item = Json::object();
        item["region_id"] = frame.regionId;
        if (frame.durationNanoseconds.has_value())
        {
            item["duration_ns"] = *frame.durationNanoseconds;
        }
        else
        {
            item["duration_ns"] = nullptr;
        }
        frames.push_back(std::move(item));
    }
    root["frames"] = std::move(frames);

    Json tags = Json::array();
    for (const SpriteImportedTag& tag : result.tags)
    {
        Json item = Json::object();
        item["name"] = tag.name;
        item["first_frame"] = tag.firstFrame;
        item["last_frame"] = tag.lastFrame;
        item["direction"] = std::string{ToString(tag.direction)};
        tags.push_back(std::move(item));
    }
    root["tags"] = std::move(tags);

    Json diagnostics = Json::array();
    for (const SpriteImportDiagnostic& diagnostic : result.diagnostics)
    {
        Json item = Json::object();
        item["code"] = std::string{ToString(diagnostic.code)};
        item["path"] = diagnostic.path;
        item["message"] = diagnostic.message;
        diagnostics.push_back(std::move(item));
    }
    root["diagnostics"] = std::move(diagnostics);
    return root.dump();
}
} // namespace trace2d::assets
