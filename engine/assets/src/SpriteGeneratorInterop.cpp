#include <trace2d/assets/SpriteGeneratorInterop.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::assets
{
namespace
{
using Json = nlohmann::ordered_json;
constexpr std::uint64_t NanosecondsPerMillisecond = 1'000'000ULL;

struct PlannedFrame final
{
    std::string regionId{};
    SpritePixelRect packedRect{};
    SpritePixelSize sourceSize{};
    SpritePixelOffset trimOffset{};
    SpritePixelSize trimSize{};
    SpriteRationalPivot pivot{};
    std::int64_t durationNanoseconds{0};
};

struct PlannedAnimation final
{
    std::string name{};
    std::uint32_t row{0};
    std::uint32_t declaredFps{0};
    bool loop{false};
    std::vector<PlannedFrame> frames{};
};

void AddDiagnostic(
    SpriteGeneratorImportResult& result,
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
    SpriteGeneratorImportResult& result)
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

bool ReadU64(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    std::uint64_t& output,
    SpriteGeneratorImportResult& result)
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
    if (!JsonUnsigned(*iterator, output))
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path + "." + std::string{key},
            "Expected a non-negative integer.");
        return false;
    }
    return true;
}

bool ReadI64(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    std::int64_t& output,
    SpriteGeneratorImportResult& result)
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

    if (iterator->is_number_unsigned())
    {
        const std::uint64_t value = iterator->get<std::uint64_t>();
        if (value <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            output = static_cast<std::int64_t>(value);
            return true;
        }
    }
    else if (iterator->is_number_integer())
    {
        output = iterator->get<std::int64_t>();
        return true;
    }

    AddDiagnostic(
        result,
        SpriteImportErrorCode::InvalidField,
        path + "." + std::string{key},
        "Expected an integer that fits int64.");
    return false;
}

bool ReadBool(
    const Json& object,
    const std::string_view key,
    const std::string& path,
    bool& output,
    SpriteGeneratorImportResult& result)
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
    SpriteGeneratorImportResult& result)
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
    const std::string& path,
    SpritePixelRect& rect,
    SpriteGeneratorImportResult& result)
{
    if (!object.is_object())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            path,
            "Expected an object with x/y/w/h.");
        return false;
    }

    bool valid = true;
    valid = ReadU32(object, "x", path, rect.x, result) && valid;
    valid = ReadU32(object, "y", path, rect.y, result) && valid;
    valid = ReadU32(object, "w", path, rect.width, result) && valid;
    valid = ReadU32(object, "h", path, rect.height, result) && valid;
    return valid;
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

bool TrimFitsCell(
    const SpritePixelRect trim,
    const std::uint32_t cellWidth,
    const std::uint32_t cellHeight) noexcept
{
    return trim.width > 0U && trim.height > 0U &&
        static_cast<std::uint64_t>(trim.x) + trim.width <= cellWidth &&
        static_cast<std::uint64_t>(trim.y) + trim.height <= cellHeight;
}

bool MillisecondsToNanoseconds(
    const std::uint64_t milliseconds,
    const std::string& path,
    std::int64_t& output,
    SpriteGeneratorImportResult& result)
{
    if (milliseconds == 0U ||
        milliseconds >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) /
                NanosecondsPerMillisecond)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDuration,
            path,
            "Duration must be positive and fit exact int64 nanoseconds.");
        return false;
    }

    output = static_cast<std::int64_t>(milliseconds * NanosecondsPerMillisecond);
    return true;
}

bool ValidateSheetIdentity(
    const std::string& manifestImage,
    const std::uint32_t manifestWidth,
    const std::uint32_t manifestHeight,
    const SpriteImportDecodedImageView& decodedSheet,
    const std::string& imagePath,
    const std::string& sizePath,
    SpriteGeneratorImportResult& result)
{
    bool valid = true;
    if (decodedSheet.id.empty() || manifestImage != decodedSheet.id)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            imagePath,
            "Manifest image must exactly match the supplied decoded sheet ID.");
        valid = false;
    }
    if (manifestWidth == 0U || manifestHeight == 0U ||
        manifestWidth != decodedSheet.width || manifestHeight != decodedSheet.height)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDimensions,
            sizePath,
            "Manifest dimensions must be positive and exactly match the supplied decoded sheet.");
        valid = false;
    }
    return valid;
}

std::string MakeRegionId(const std::string& animationName, const std::size_t frameIndex)
{
    return animationName + "/frame-" + std::to_string(frameIndex);
}

bool ValidateAndSortAnimations(
    std::vector<PlannedAnimation>& animations,
    SpriteGeneratorImportResult& result)
{
    if (animations.empty())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::ExpectedFrameCountMismatch,
            "$.animations",
            "Manifest must contain at least one animation with at least one frame.");
        return false;
    }

    std::sort(
        animations.begin(),
        animations.end(),
        [](const PlannedAnimation& lhs, const PlannedAnimation& rhs)
        {
            if (lhs.row != rhs.row)
            {
                return lhs.row < rhs.row;
            }
            return lhs.name < rhs.name;
        });

    bool valid = true;
    for (std::size_t index = 0U; index < animations.size(); ++index)
    {
        if (animations[index].row != index)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                "$.animations." + animations[index].name + ".row",
                "Animation rows must be unique and contiguous from zero.");
            valid = false;
        }
        if (animations[index].frames.empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::ExpectedFrameCountMismatch,
                "$.animations." + animations[index].name,
                "Animation must contain at least one frame.");
            valid = false;
        }
    }
    return valid;
}

bool PlanSpriteGen(
    const Json& root,
    const SpriteImportDecodedImageView& decodedSheet,
    const SpriteGeneratorManifestImportOptions& options,
    std::vector<PlannedAnimation>& animations,
    SpriteGeneratorImportResult& result)
{
    if (!options.spriteGenDefaultPivot.has_value())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::MissingField,
            "$.options.sprite_gen_default_pivot",
            "sprite-gen import requires an explicit rational pivot; SPP4 never infers one from pixels.");
        return false;
    }
    if (options.spriteGenDefaultPivot->denominator <= 0)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidField,
            "$.options.sprite_gen_default_pivot.denominator",
            "Explicit sprite-gen pivot denominator must be positive.");
        return false;
    }

    std::string engine{};
    std::string gameInput{};
    bool degradedFallback = true;
    bool valid = true;
    valid = ReadString(root, "engine", "$", engine, result) && valid;
    valid = ReadString(root, "game_input", "$", gameInput, result) && valid;
    valid = ReadBool(root, "degraded_static_fallback", "$", degradedFallback, result) && valid;
    if (!valid)
    {
        return false;
    }
    if (engine != "component-row")
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$.engine",
            "SPP4 supports sprite-gen component-row runtime manifests only.");
        valid = false;
    }
    if (degradedFallback)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$.degraded_static_fallback",
            "degraded_static_fallback must be false.");
        valid = false;
    }

    const auto layoutIterator = root.find("frame_layout");
    const auto animationIterator = root.find("animation");
    if (layoutIterator == root.end() || !layoutIterator->is_object())
    {
        AddDiagnostic(
            result,
            layoutIterator == root.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            "$.frame_layout",
            "sprite-gen manifest requires a frame_layout object.");
        valid = false;
    }
    if (animationIterator == root.end() || !animationIterator->is_object())
    {
        AddDiagnostic(
            result,
            animationIterator == root.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            "$.animation",
            "sprite-gen manifest requires an animation object.");
        valid = false;
    }
    if (!valid)
    {
        return false;
    }

    const Json& layout = *layoutIterator;
    const Json& animation = *animationIterator;
    std::uint32_t sheetWidth = 0U;
    std::uint32_t sheetHeight = 0U;
    std::uint32_t cellWidth = 0U;
    std::uint32_t cellHeight = 0U;
    valid = ReadU32(layout, "sheetWidth", "$.frame_layout", sheetWidth, result) && valid;
    valid = ReadU32(layout, "sheetHeight", "$.frame_layout", sheetHeight, result) && valid;
    valid = ReadU32(layout, "cellWidth", "$.frame_layout", cellWidth, result) && valid;
    valid = ReadU32(layout, "cellHeight", "$.frame_layout", cellHeight, result) && valid;
    if (cellWidth == 0U || cellHeight == 0U)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDimensions,
            "$.frame_layout",
            "sprite-gen cell dimensions must be positive.");
        valid = false;
    }
    valid = ValidateSheetIdentity(
        gameInput,
        sheetWidth,
        sheetHeight,
        decodedSheet,
        "$.game_input",
        "$.frame_layout",
        result) && valid;

    const auto frameRowsIterator = layout.find("rows");
    const auto animationRowsIterator = animation.find("rows");
    if (frameRowsIterator == layout.end() || !frameRowsIterator->is_object())
    {
        AddDiagnostic(
            result,
            frameRowsIterator == layout.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            "$.frame_layout.rows",
            "frame_layout.rows must be an object.");
        valid = false;
    }
    if (animationRowsIterator == animation.end() || !animationRowsIterator->is_object())
    {
        AddDiagnostic(
            result,
            animationRowsIterator == animation.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            "$.animation.rows",
            "animation.rows must be an object.");
        valid = false;
    }
    if (!valid)
    {
        return false;
    }

    const Json& frameRows = *frameRowsIterator;
    const Json& animationRows = *animationRowsIterator;
    if (frameRows.size() != animationRows.size())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::ExpectedFrameCountMismatch,
            "$.frame_layout.rows",
            "frame_layout.rows and animation.rows must contain the same state set.");
        return false;
    }

    for (auto iterator = animationRows.begin(); iterator != animationRows.end(); ++iterator)
    {
        const std::string name = iterator.key();
        const std::string path = "$.animation.rows." + name;
        if (name.empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path,
                "Animation/state name must not be empty.");
            valid = false;
            continue;
        }
        if (!iterator.value().is_object())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path,
                "Animation row must be an object.");
            valid = false;
            continue;
        }

        const auto framesIterator = frameRows.find(name);
        if (framesIterator == frameRows.end() || !framesIterator->is_array())
        {
            AddDiagnostic(
                result,
                framesIterator == frameRows.end()
                    ? SpriteImportErrorCode::MissingField
                    : SpriteImportErrorCode::InvalidField,
                "$.frame_layout.rows." + name,
                "Every animation row requires a matching frame-layout array.");
            valid = false;
            continue;
        }

        const Json& entry = iterator.value();
        PlannedAnimation planned{};
        planned.name = name;
        std::uint32_t declaredFrames = 0U;
        bool entryValid = true;
        entryValid = ReadU32(entry, "row", path, planned.row, result) && entryValid;
        entryValid = ReadU32(entry, "frames", path, declaredFrames, result) && entryValid;
        entryValid = ReadU32(entry, "fps", path, planned.declaredFps, result) && entryValid;
        entryValid = ReadBool(entry, "loop", path, planned.loop, result) && entryValid;
        if (planned.declaredFps == 0U)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path + ".fps",
                "Declared FPS must be positive.");
            entryValid = false;
        }

        const auto durationsIterator = entry.find("durations_ms");
        if (durationsIterator == entry.end() || !durationsIterator->is_array())
        {
            AddDiagnostic(
                result,
                durationsIterator == entry.end()
                    ? SpriteImportErrorCode::MissingField
                    : SpriteImportErrorCode::InvalidField,
                path + ".durations_ms",
                "durations_ms must be an array.");
            entryValid = false;
        }

        if (!entryValid)
        {
            valid = false;
            continue;
        }

        const Json& frameRects = *framesIterator;
        const Json& durations = *durationsIterator;
        if (declaredFrames == 0U ||
            declaredFrames != frameRects.size() ||
            declaredFrames != durations.size())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::ExpectedFrameCountMismatch,
                path + ".frames",
                "Declared frames, frame_layout row length and durations_ms length must agree and be positive.");
            valid = false;
            continue;
        }

        planned.frames.reserve(declaredFrames);
        for (std::size_t frameIndex = 0U; frameIndex < frameRects.size(); ++frameIndex)
        {
            const std::string framePath =
                "$.frame_layout.rows." + name + "[" + std::to_string(frameIndex) + "]";
            SpritePixelRect rect{};
            bool frameValid = ReadRect(frameRects[frameIndex], framePath, rect, result);
            if (frameValid &&
                (rect.width != cellWidth || rect.height != cellHeight))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::InvalidRectangle,
                    framePath,
                    "sprite-gen frame-layout rectangles must match the declared cell dimensions.");
                frameValid = false;
            }
            if (frameValid && !RectFits(rect, sheetWidth, sheetHeight))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::RectangleOutOfBounds,
                    framePath,
                    "sprite-gen frame-layout rectangle must be non-empty and inside the atlas.");
                frameValid = false;
            }

            std::uint64_t durationMs = 0U;
            const Json& durationValue = durations[frameIndex];
            if (!JsonUnsigned(durationValue, durationMs))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::InvalidDuration,
                    path + ".durations_ms[" + std::to_string(frameIndex) + "]",
                    "Duration must be a non-negative integer.");
                frameValid = false;
            }
            std::int64_t durationNs = 0;
            if (frameValid &&
                !MillisecondsToNanoseconds(
                    durationMs,
                    path + ".durations_ms[" + std::to_string(frameIndex) + "]",
                    durationNs,
                    result))
            {
                frameValid = false;
            }

            if (!frameValid)
            {
                valid = false;
                continue;
            }

            planned.frames.push_back(PlannedFrame{
                .regionId = MakeRegionId(name, frameIndex),
                .packedRect = rect,
                .sourceSize = SpritePixelSize{rect.width, rect.height},
                .trimOffset = SpritePixelOffset{},
                .trimSize = SpritePixelSize{rect.width, rect.height},
                .pivot = *options.spriteGenDefaultPivot,
                .durationNanoseconds = durationNs,
            });
        }
        if (planned.frames.size() == declaredFrames)
        {
            animations.push_back(std::move(planned));
        }
        else
        {
            valid = false;
        }
    }

    if (!valid)
    {
        return false;
    }
    return ValidateAndSortAnimations(animations, result);
}

bool PlanPerfectPixel(
    const Json& root,
    const SpriteImportDecodedImageView& decodedSheet,
    std::vector<PlannedAnimation>& animations,
    SpriteGeneratorImportResult& result)
{
    std::string app{};
    std::string schema{};
    std::uint32_t version = 0U;
    bool valid = true;
    valid = ReadString(root, "app", "$", app, result) && valid;
    valid = ReadString(root, "schema", "$", schema, result) && valid;
    valid = ReadU32(root, "version", "$", version, result) && valid;
    if (!valid)
    {
        return false;
    }
    if (app != "perfectpixel" || schema != "perfectpixel.sprite/2" || version != 2U)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$",
            "SPP4 supports app=perfectpixel, schema=perfectpixel.sprite/2, version=2 only.");
        return false;
    }

    const auto sheetIterator = root.find("sheet");
    const auto animationsIterator = root.find("animations");
    if (sheetIterator == root.end() || !sheetIterator->is_object())
    {
        AddDiagnostic(
            result,
            sheetIterator == root.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            "$.sheet",
            "PerfectPixel manifest requires a sheet object.");
        valid = false;
    }
    if (animationsIterator == root.end() || !animationsIterator->is_object())
    {
        AddDiagnostic(
            result,
            animationsIterator == root.end()
                ? SpriteImportErrorCode::MissingField
                : SpriteImportErrorCode::InvalidField,
            "$.animations",
            "PerfectPixel manifest requires an animations object.");
        valid = false;
    }
    if (!valid)
    {
        return false;
    }

    const Json& sheet = *sheetIterator;
    std::string image{};
    std::uint32_t sheetWidth = 0U;
    std::uint32_t sheetHeight = 0U;
    std::uint32_t cellWidth = 0U;
    std::uint32_t cellHeight = 0U;
    valid = ReadString(sheet, "image", "$.sheet", image, result) && valid;
    valid = ReadU32(sheet, "width", "$.sheet", sheetWidth, result) && valid;
    valid = ReadU32(sheet, "height", "$.sheet", sheetHeight, result) && valid;
    valid = ReadU32(sheet, "cellWidth", "$.sheet", cellWidth, result) && valid;
    valid = ReadU32(sheet, "cellHeight", "$.sheet", cellHeight, result) && valid;
    if (cellWidth == 0U || cellHeight == 0U)
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::InvalidDimensions,
            "$.sheet",
            "PerfectPixel cell dimensions must be positive.");
        valid = false;
    }
    valid = ValidateSheetIdentity(
        image,
        sheetWidth,
        sheetHeight,
        decodedSheet,
        "$.sheet.image",
        "$.sheet",
        result) && valid;
    if (!valid)
    {
        return false;
    }

    const Json& manifestAnimations = *animationsIterator;
    for (auto iterator = manifestAnimations.begin(); iterator != manifestAnimations.end(); ++iterator)
    {
        const std::string name = iterator.key();
        const std::string path = "$.animations." + name;
        if (name.empty())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path,
                "Animation name must not be empty.");
            valid = false;
            continue;
        }
        if (!iterator.value().is_object())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path,
                "Animation entry must be an object.");
            valid = false;
            continue;
        }

        const Json& entry = iterator.value();
        PlannedAnimation planned{};
        planned.name = name;
        std::uint32_t declaredFrames = 0U;
        std::uint64_t durationMs = 0U;
        bool entryValid = true;
        entryValid = ReadU32(entry, "row", path, planned.row, result) && entryValid;
        entryValid = ReadU32(entry, "frames", path, declaredFrames, result) && entryValid;
        entryValid = ReadU32(entry, "fps", path, planned.declaredFps, result) && entryValid;
        entryValid = ReadBool(entry, "loop", path, planned.loop, result) && entryValid;
        entryValid = ReadU64(entry, "durationMs", path, durationMs, result) && entryValid;
        if (planned.declaredFps == 0U)
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::InvalidField,
                path + ".fps",
                "Declared FPS must be positive.");
            entryValid = false;
        }

        const auto pivotIterator = entry.find("pivot");
        const auto rectsIterator = entry.find("rects");
        const auto trimsIterator = entry.find("trims");
        if (pivotIterator == entry.end() || !pivotIterator->is_object())
        {
            AddDiagnostic(
                result,
                pivotIterator == entry.end()
                    ? SpriteImportErrorCode::MissingField
                    : SpriteImportErrorCode::InvalidField,
                path + ".pivot",
                "PerfectPixel animation requires an integer pivot object.");
            entryValid = false;
        }
        if (rectsIterator == entry.end() || !rectsIterator->is_array())
        {
            AddDiagnostic(
                result,
                rectsIterator == entry.end()
                    ? SpriteImportErrorCode::MissingField
                    : SpriteImportErrorCode::InvalidField,
                path + ".rects",
                "PerfectPixel rects must be an array.");
            entryValid = false;
        }
        if (trimsIterator == entry.end() || !trimsIterator->is_array())
        {
            AddDiagnostic(
                result,
                trimsIterator == entry.end()
                    ? SpriteImportErrorCode::MissingField
                    : SpriteImportErrorCode::InvalidField,
                path + ".trims",
                "PerfectPixel trims must be an array.");
            entryValid = false;
        }

        std::int64_t pivotX = 0;
        std::int64_t pivotY = 0;
        if (pivotIterator != entry.end() && pivotIterator->is_object())
        {
            entryValid = ReadI64(*pivotIterator, "x", path + ".pivot", pivotX, result) && entryValid;
            entryValid = ReadI64(*pivotIterator, "y", path + ".pivot", pivotY, result) && entryValid;
        }

        std::int64_t durationNs = 0;
        if (entryValid &&
            !MillisecondsToNanoseconds(durationMs, path + ".durationMs", durationNs, result))
        {
            entryValid = false;
        }
        if (!entryValid)
        {
            valid = false;
            continue;
        }

        const Json& rects = *rectsIterator;
        const Json& trims = *trimsIterator;
        if (declaredFrames == 0U ||
            declaredFrames != rects.size() ||
            declaredFrames != trims.size())
        {
            AddDiagnostic(
                result,
                SpriteImportErrorCode::ExpectedFrameCountMismatch,
                path + ".frames",
                "Declared frames, rects length and trims length must agree and be positive.");
            valid = false;
            continue;
        }

        planned.frames.reserve(declaredFrames);
        for (std::size_t frameIndex = 0U; frameIndex < rects.size(); ++frameIndex)
        {
            const std::string rectPath =
                path + ".rects[" + std::to_string(frameIndex) + "]";
            const std::string trimPath =
                path + ".trims[" + std::to_string(frameIndex) + "]";
            SpritePixelRect cellRect{};
            SpritePixelRect trim{};
            bool frameValid = ReadRect(rects[frameIndex], rectPath, cellRect, result);
            frameValid = ReadRect(trims[frameIndex], trimPath, trim, result) && frameValid;

            if (frameValid &&
                (cellRect.width != cellWidth || cellRect.height != cellHeight))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::InvalidRectangle,
                    rectPath,
                    "PerfectPixel frame rect must match sheet cell dimensions.");
                frameValid = false;
            }
            if (frameValid && !RectFits(cellRect, sheetWidth, sheetHeight))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::RectangleOutOfBounds,
                    rectPath,
                    "PerfectPixel frame cell must be non-empty and inside the atlas.");
                frameValid = false;
            }
            if (frameValid && !TrimFitsCell(trim, cellWidth, cellHeight))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::InvalidRectangle,
                    trimPath,
                    "PerfectPixel trim must be non-empty and inside its frame cell.");
                frameValid = false;
            }
            if (!frameValid)
            {
                valid = false;
                continue;
            }

            const std::uint64_t packedX =
                static_cast<std::uint64_t>(cellRect.x) + trim.x;
            const std::uint64_t packedY =
                static_cast<std::uint64_t>(cellRect.y) + trim.y;
            if (packedX > std::numeric_limits<std::uint32_t>::max() ||
                packedY > std::numeric_limits<std::uint32_t>::max())
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::SizeOverflow,
                    trimPath,
                    "Derived packed content origin overflows uint32.");
                valid = false;
                continue;
            }

            const SpritePixelRect packed{
                static_cast<std::uint32_t>(packedX),
                static_cast<std::uint32_t>(packedY),
                trim.width,
                trim.height,
            };
            if (!RectFits(packed, sheetWidth, sheetHeight))
            {
                AddDiagnostic(
                    result,
                    SpriteImportErrorCode::RectangleOutOfBounds,
                    trimPath,
                    "Derived PerfectPixel packed content must stay inside the atlas.");
                valid = false;
                continue;
            }

            planned.frames.push_back(PlannedFrame{
                .regionId = MakeRegionId(name, frameIndex),
                .packedRect = packed,
                .sourceSize = SpritePixelSize{cellWidth, cellHeight},
                .trimOffset = SpritePixelOffset{trim.x, trim.y},
                .trimSize = SpritePixelSize{trim.width, trim.height},
                .pivot = SpriteRationalPivot{pivotX, pivotY, 1},
                .durationNanoseconds = durationNs,
            });
        }
        if (planned.frames.size() == declaredFrames)
        {
            animations.push_back(std::move(planned));
        }
        else
        {
            valid = false;
        }
    }

    if (!valid)
    {
        return false;
    }
    return ValidateAndSortAnimations(animations, result);
}

bool LowerThroughGenericImporter(
    const std::vector<PlannedAnimation>& planned,
    const SpriteImportDecodedImageView& decodedSheet,
    const SpriteGeneratorManifestImportOptions& options,
    SpriteGeneratorImportResult& result)
{
    std::uint64_t totalFrames = 0U;
    for (const PlannedAnimation& animation : planned)
    {
        totalFrames += animation.frames.size();
    }
    if (totalFrames == 0U || totalFrames > std::numeric_limits<std::uint32_t>::max())
    {
        AddDiagnostic(
            result,
            totalFrames == 0U
                ? SpriteImportErrorCode::ExpectedFrameCountMismatch
                : SpriteImportErrorCode::SizeOverflow,
            "$.animations",
            "Total imported frame count must be positive and fit uint32.");
        return false;
    }

    std::vector<SpriteGenericRegionView> regions{};
    std::vector<std::int64_t> durations{};
    regions.reserve(static_cast<std::size_t>(totalFrames));
    durations.reserve(static_cast<std::size_t>(totalFrames));
    result.animations.reserve(planned.size());

    std::uint32_t firstFrame = 0U;
    for (const PlannedAnimation& animation : planned)
    {
        result.animations.push_back(SpriteGeneratorAnimationEvidence{
            .name = animation.name,
            .row = animation.row,
            .firstFrame = firstFrame,
            .frameCount = static_cast<std::uint32_t>(animation.frames.size()),
            .declaredFps = animation.declaredFps,
            .loop = animation.loop,
        });

        for (const PlannedFrame& frame : animation.frames)
        {
            regions.push_back(SpriteGenericRegionView{
                .id = frame.regionId,
                .packedRect = frame.packedRect,
                .sourceSize = frame.sourceSize,
                .trimOffset = frame.trimOffset,
                .trimSize = frame.trimSize,
                .pivot = frame.pivot,
                .packedRotation = SpritePackedRotation::None,
            });
            durations.push_back(frame.durationNanoseconds);
        }
        firstFrame += static_cast<std::uint32_t>(animation.frames.size());
    }

    const SpriteGenericSheetImportSpec spec{
        .mode = SpriteGenericSheetMode::ExplicitRegions,
        .explicitRegions =
            std::span<const SpriteGenericRegionView>{regions.data(), regions.size()},
        .grid = {},
        .gridRegionIds = {},
        .expectedFrameCount = static_cast<std::uint32_t>(totalFrames),
        .defaultPivot = {},
    };
    const SpriteGenericSheetImportOptions genericOptions{
        .canonicalAssetId = options.canonicalAssetId,
        .pageId = options.pageId,
        .textureReference = options.textureReference,
        .sampling = options.sampling,
        .colorSpace = options.colorSpace,
    };

    result.canonicalImport =
        ImportGenericSpriteSheet(decodedSheet, spec, genericOptions);
    if (!result.canonicalImport.Succeeded())
    {
        result.animations.clear();
        return false;
    }
    if (result.canonicalImport.frames.size() != durations.size())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::ExpectedFrameCountMismatch,
            "$.canonical_import.frames",
            "SPP3 generic importer returned an unexpected frame count.");
        result.canonicalImport = {};
        result.animations.clear();
        return false;
    }

    for (std::size_t index = 0U; index < durations.size(); ++index)
    {
        result.canonicalImport.frames[index].durationNanoseconds = durations[index];
    }
    return true;
}

void WriteDiagnostics(Json& output, const std::vector<SpriteImportDiagnostic>& diagnostics)
{
    output = Json::array();
    for (const SpriteImportDiagnostic& diagnostic : diagnostics)
    {
        Json item = Json::object();
        item["code"] = std::string{ToString(diagnostic.code)};
        item["path"] = diagnostic.path;
        item["message"] = diagnostic.message;
        output.push_back(std::move(item));
    }
}
} // namespace

std::string_view ToString(const SpriteGeneratorManifestKind value) noexcept
{
    switch (value)
    {
    case SpriteGeneratorManifestKind::SpriteGenComponentRow:
        return "sprite_gen_component_row";
    case SpriteGeneratorManifestKind::PerfectPixelV2:
        return "perfectpixel_v2";
    }
    return "unknown";
}

SpriteGeneratorImportResult ImportSpriteGeneratorManifestJson(
    const SpriteGeneratorManifestKind kind,
    const std::string_view jsonText,
    const SpriteImportDecodedImageView& decodedSheet,
    const SpriteGeneratorManifestImportOptions& options)
{
    SpriteGeneratorImportResult result{};
    result.manifestKind = kind;

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
            std::string{"Generator manifest JSON parse failed: "} + error.what());
        return result;
    }
    if (!root.is_object())
    {
        AddDiagnostic(
            result,
            SpriteImportErrorCode::UnsupportedManifest,
            "$",
            "Generator manifest root must be an object.");
        return result;
    }

    std::vector<PlannedAnimation> planned{};
    bool plannedSuccessfully = false;
    switch (kind)
    {
    case SpriteGeneratorManifestKind::SpriteGenComponentRow:
        plannedSuccessfully =
            PlanSpriteGen(root, decodedSheet, options, planned, result);
        break;
    case SpriteGeneratorManifestKind::PerfectPixelV2:
        plannedSuccessfully =
            PlanPerfectPixel(root, decodedSheet, planned, result);
        break;
    }

    if (!plannedSuccessfully || !result.diagnostics.empty())
    {
        return result;
    }

    LowerThroughGenericImporter(planned, decodedSheet, options, result);
    return result;
}

std::string SerializeSpriteGeneratorImportResultJson(
    const SpriteGeneratorImportResult& result)
{
    Json root = Json::object();
    root["schema_version"] = result.schemaVersion;
    root["manifest_kind"] = std::string{ToString(result.manifestKind)};

    Json animations = Json::array();
    for (const SpriteGeneratorAnimationEvidence& animation : result.animations)
    {
        Json item = Json::object();
        item["name"] = animation.name;
        item["row"] = animation.row;
        item["first_frame"] = animation.firstFrame;
        item["frame_count"] = animation.frameCount;
        item["declared_fps"] = animation.declaredFps;
        item["loop"] = animation.loop;
        animations.push_back(std::move(item));
    }
    root["animations"] = std::move(animations);

    Json diagnostics{};
    WriteDiagnostics(diagnostics, result.diagnostics);
    root["diagnostics"] = std::move(diagnostics);

    root["canonical_import"] =
        Json::parse(SerializeSpriteImportResultJson(result.canonicalImport));
    return root.dump();
}
} // namespace trace2d::assets
