#include <trace2d/assets/SpriteProcessing.hpp>
#include <trace2d/assets/TextureAssets.hpp>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
void PrintUsage()
{
    std::cout
        << "Usage: trace2d_sprite_process [options] <project-relative-image> [image ...]\n"
        << "Options:\n"
        << "  --project-root <path>             Project root used by TextureAssetCache (default: .)\n"
        << "  --grid-columns <count>            Record explicit frame-grid column evidence\n"
        << "  --max-bounds-displacement <px>    Warn when adjacent alpha-bounds origins move farther\n"
        << "  --require-uniform-pivot           Require supplied pivots to match (CLI images have none)\n"
        << "  --help                            Show this help\n";
}

bool ParseUint32(const std::string_view text, std::uint32_t& value)
{
    if (text.empty())
    {
        return false;
    }

    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

int FailArgument(const std::string_view message)
{
    std::cerr << "trace2d_sprite_process: " << message << '\n';
    return 2;
}
} // namespace

int main(const int argc, char** argv)
{
    std::filesystem::path projectRoot{"."};
    trace2d::assets::SpriteProcessingOptions options{};
    std::vector<std::string> references{};

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help")
        {
            PrintUsage();
            return 0;
        }
        if (argument == "--project-root")
        {
            if (index + 1 >= argc)
            {
                return FailArgument("--project-root requires a path.");
            }
            projectRoot = std::filesystem::path{argv[++index]};
            continue;
        }
        if (argument == "--grid-columns")
        {
            if (index + 1 >= argc)
            {
                return FailArgument("--grid-columns requires a positive integer.");
            }
            std::uint32_t columns = 0U;
            if (!ParseUint32(argv[++index], columns) || columns == 0U)
            {
                return FailArgument("--grid-columns requires a positive uint32 value.");
            }
            options.gridColumns = columns;
            continue;
        }
        if (argument == "--max-bounds-displacement")
        {
            if (index + 1 >= argc)
            {
                return FailArgument("--max-bounds-displacement requires a non-negative integer.");
            }
            std::uint32_t pixels = 0U;
            if (!ParseUint32(argv[++index], pixels))
            {
                return FailArgument("--max-bounds-displacement requires a uint32 value.");
            }
            options.maxBoundsOriginDisplacementPixels = pixels;
            continue;
        }
        if (argument == "--require-uniform-pivot")
        {
            options.requireUniformPivot = true;
            continue;
        }
        if (!argument.empty() && argument.front() == '-')
        {
            return FailArgument("unknown option: " + std::string{argument});
        }

        references.emplace_back(argument);
    }

    if (references.empty())
    {
        PrintUsage();
        return FailArgument("at least one project-relative image is required.");
    }

    trace2d::assets::TextureAssetCache textureCache{std::move(projectRoot)};
    std::vector<std::shared_ptr<const trace2d::assets::TextureAssetData>> loaded{};
    loaded.reserve(references.size());

    for (const std::string& reference : references)
    {
        trace2d::assets::TextureAssetLoadResult load = textureCache.Load(reference);
        if (!load.Succeeded())
        {
            if (load.diagnostic.has_value())
            {
                std::cerr << "trace2d_sprite_process: "
                          << trace2d::assets::ToString(load.diagnostic->code)
                          << ": " << load.diagnostic->message << '\n';
            }
            else
            {
                std::cerr << "trace2d_sprite_process: texture load failed without a diagnostic.\n";
            }
            return 3;
        }
        loaded.push_back(std::move(load.asset));
    }

    std::vector<trace2d::assets::SpriteProcessingFrameView> frames{};
    frames.reserve(loaded.size());
    for (const auto& image : loaded)
    {
        frames.push_back(trace2d::assets::SpriteProcessingFrameView{
            .id = image->id,
            .width = image->width,
            .height = image->height,
            .rgba8 = image->rgba8,
        });
    }

    const trace2d::assets::SpriteProcessingResult result =
        trace2d::assets::AnalyzeSpriteProcessing(
            std::span<const trace2d::assets::SpriteProcessingFrameView>{frames},
            {},
            options);
    if (!result.Succeeded())
    {
        for (const trace2d::assets::SpriteProcessingDiagnostic& diagnostic : result.diagnostics)
        {
            std::cerr << "trace2d_sprite_process: "
                      << trace2d::assets::ToString(diagnostic.code)
                      << ": " << diagnostic.id << ": " << diagnostic.message << '\n';
        }
        return 4;
    }

    std::cout << trace2d::assets::SerializeSpriteProcessingReportJson(*result.report) << '\n';
    return 0;
}
