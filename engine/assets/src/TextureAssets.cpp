#include <trace2d/assets/TextureAssets.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include <stb_image.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace trace2d::assets
{
namespace
{
struct NormalizedReference final
{
    std::string id{};
    std::filesystem::path resolvedPath{};
};

TextureAssetDiagnostic MakeDiagnostic(
    const TextureAssetErrorCode code,
    const std::string_view reference,
    const std::filesystem::path& resolvedPath,
    std::string message)
{
    TextureAssetDiagnostic diagnostic{};
    diagnostic.code = code;
    diagnostic.reference = std::string{reference};
    diagnostic.resolvedPath = resolvedPath.empty() ? std::string{} : resolvedPath.generic_string();
    diagnostic.message = std::move(message);
    return diagnostic;
}

bool IsAsciiDrivePrefix(const std::string_view reference) noexcept
{
    if (reference.size() < 2U || reference[1] != ':')
    {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(reference[0]);
    return std::isalpha(first) != 0;
}

bool TryNormalizeReference(
    const std::filesystem::path& projectRoot,
    const std::string_view reference,
    NormalizedReference& normalized,
    TextureAssetDiagnostic* diagnostic)
{
    if (reference.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                TextureAssetErrorCode::InvalidReference,
                reference,
                {},
                "Texture reference must not be empty.");
        }
        return false;
    }

    std::string portableReference{reference};
    std::replace(portableReference.begin(), portableReference.end(), '\\', '/');

    if (portableReference.front() == '/' || IsAsciiDrivePrefix(portableReference))
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                TextureAssetErrorCode::InvalidReference,
                reference,
                {},
                "Texture reference must be project-relative, not absolute.");
        }
        return false;
    }

    std::string canonical{};
    canonical.reserve(portableReference.size());

    std::size_t cursor = 0U;
    while (cursor <= portableReference.size())
    {
        const std::size_t separator = portableReference.find('/', cursor);
        const std::size_t end = separator == std::string::npos ? portableReference.size() : separator;
        const std::string_view component{portableReference.data() + cursor, end - cursor};

        if (!component.empty() && component != ".")
        {
            if (component == "..")
            {
                if (diagnostic != nullptr)
                {
                    *diagnostic = MakeDiagnostic(
                        TextureAssetErrorCode::InvalidReference,
                        reference,
                        {},
                        "Texture reference must not traverse outside the project root.");
                }
                return false;
            }

            if (component.find('\0') != std::string_view::npos)
            {
                if (diagnostic != nullptr)
                {
                    *diagnostic = MakeDiagnostic(
                        TextureAssetErrorCode::InvalidReference,
                        reference,
                        {},
                        "Texture reference contains an embedded null character.");
                }
                return false;
            }

            if (!canonical.empty())
            {
                canonical.push_back('/');
            }
            canonical.append(component);
        }

        if (separator == std::string::npos)
        {
            break;
        }
        cursor = separator + 1U;
    }

    if (canonical.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                TextureAssetErrorCode::InvalidReference,
                reference,
                {},
                "Texture reference must identify a file below the project root.");
        }
        return false;
    }

    normalized.id = std::move(canonical);
    normalized.resolvedPath = (projectRoot / std::filesystem::path{normalized.id}).lexically_normal();
    return true;
}

bool IsSupportedTextureExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](const unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
           extension == ".bmp" || extension == ".tga";
}

TextureAssetLoadResult Failure(TextureAssetDiagnostic diagnostic)
{
    TextureAssetLoadResult result{};
    result.diagnostic = std::move(diagnostic);
    return result;
}
} // namespace

std::string_view ToString(const TextureAssetErrorCode code) noexcept
{
    switch (code)
    {
    case TextureAssetErrorCode::InvalidReference:
        return "invalid_reference";
    case TextureAssetErrorCode::UnsupportedFormat:
        return "unsupported_format";
    case TextureAssetErrorCode::MissingFile:
        return "missing_file";
    case TextureAssetErrorCode::ReadFailure:
        return "read_failure";
    case TextureAssetErrorCode::DecodeFailure:
        return "decode_failure";
    case TextureAssetErrorCode::SizeOverflow:
        return "size_overflow";
    }

    return "unknown";
}

TextureAssetCache::TextureAssetCache(std::filesystem::path projectRoot)
    : projectRoot_{std::move(projectRoot).lexically_normal()}
{
    if (projectRoot_.empty())
    {
        projectRoot_ = std::filesystem::path{"."};
    }
}

TextureAssetLoadResult TextureAssetCache::Load(const std::string_view projectRelativeReference)
{
    ++requests_;

    NormalizedReference normalized{};
    TextureAssetDiagnostic normalizationDiagnostic{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, &normalizationDiagnostic))
    {
        ++failedImports_;
        return Failure(std::move(normalizationDiagnostic));
    }

    const auto cached = cache_.find(normalized.id);
    if (cached != cache_.end())
    {
        ++cacheHits_;
        TextureAssetLoadResult result{};
        result.asset = cached->second;
        return result;
    }

    ++cacheMisses_;

    if (!IsSupportedTextureExtension(normalized.resolvedPath))
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::UnsupportedFormat,
            projectRelativeReference,
            normalized.resolvedPath,
            "Supported texture formats are PNG, JPEG, BMP, and TGA."));
    }

    std::error_code filesystemError{};
    const bool exists = std::filesystem::exists(normalized.resolvedPath, filesystemError);
    if (filesystemError)
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::ReadFailure,
            projectRelativeReference,
            normalized.resolvedPath,
            "Unable to inspect the texture source file: " + filesystemError.message()));
    }

    if (!exists)
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::MissingFile,
            projectRelativeReference,
            normalized.resolvedPath,
            "Texture source file does not exist."));
    }

    if (!std::filesystem::is_regular_file(normalized.resolvedPath, filesystemError) || filesystemError)
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::ReadFailure,
            projectRelativeReference,
            normalized.resolvedPath,
            "Texture source path must resolve to a regular file."));
    }

    const std::uintmax_t fileSize = std::filesystem::file_size(normalized.resolvedPath, filesystemError);
    if (filesystemError)
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::ReadFailure,
            projectRelativeReference,
            normalized.resolvedPath,
            "Unable to determine texture source size: " + filesystemError.message()));
    }

    if (fileSize > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()) ||
        fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::SizeOverflow,
            projectRelativeReference,
            normalized.resolvedPath,
            "Texture source is too large for the current decoder contract."));
    }

    std::vector<std::uint8_t> encoded(static_cast<std::size_t>(fileSize));
    std::ifstream input{normalized.resolvedPath, std::ios::binary};
    if (!input)
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::ReadFailure,
            projectRelativeReference,
            normalized.resolvedPath,
            "Unable to open texture source for reading."));
    }

    if (!encoded.empty())
    {
        input.read(
            reinterpret_cast<char*>(encoded.data()),
            static_cast<std::streamsize>(encoded.size()));
    }

    if (!input || input.gcount() != static_cast<std::streamsize>(encoded.size()))
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::ReadFailure,
            projectRelativeReference,
            normalized.resolvedPath,
            "Texture source could not be read completely."));
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    using StbiImage = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    StbiImage decoded{
        stbi_load_from_memory(
            encoded.data(),
            static_cast<int>(encoded.size()),
            &width,
            &height,
            &sourceChannels,
            STBI_rgb_alpha),
        &stbi_image_free};

    if (decoded == nullptr || width <= 0 || height <= 0)
    {
        ++failedImports_;
        const char* const reason = stbi_failure_reason();
        std::string message{"Texture decoder rejected the source file."};
        if (reason != nullptr && reason[0] != '\0')
        {
            message.append(" Decoder: ");
            message.append(reason);
        }

        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::DecodeFailure,
            projectRelativeReference,
            normalized.resolvedPath,
            std::move(message)));
    }

    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    constexpr std::uint64_t BytesPerPixel = 4U;
    if (pixelCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / BytesPerPixel)
    {
        ++failedImports_;
        return Failure(MakeDiagnostic(
            TextureAssetErrorCode::SizeOverflow,
            projectRelativeReference,
            normalized.resolvedPath,
            "Decoded RGBA8 texture size exceeds addressable memory."));
    }

    const std::size_t byteCount = static_cast<std::size_t>(pixelCount * BytesPerPixel);
    auto imported = std::make_shared<TextureAssetData>();
    imported->id = normalized.id;
    imported->width = static_cast<std::uint32_t>(width);
    imported->height = static_cast<std::uint32_t>(height);
    imported->rgba8.assign(decoded.get(), decoded.get() + byteCount);

    const std::shared_ptr<const TextureAssetData> immutableAsset = imported;
    cache_.emplace(normalized.id, immutableAsset);
    ++successfulImports_;

    TextureAssetLoadResult result{};
    result.asset = immutableAsset;
    return result;
}

bool TextureAssetCache::Invalidate(const std::string_view projectRelativeReference)
{
    NormalizedReference normalized{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, nullptr))
    {
        return false;
    }

    return cache_.erase(normalized.id) != 0U;
}

void TextureAssetCache::Clear() noexcept
{
    cache_.clear();
}

const std::filesystem::path& TextureAssetCache::ProjectRoot() const noexcept
{
    return projectRoot_;
}

TextureAssetCacheMetrics TextureAssetCache::Metrics() const noexcept
{
    TextureAssetCacheMetrics metrics{};
    metrics.requests = requests_;
    metrics.cacheHits = cacheHits_;
    metrics.cacheMisses = cacheMisses_;
    metrics.successfulImports = successfulImports_;
    metrics.failedImports = failedImports_;
    metrics.cachedAssets = cache_.size();
    return metrics;
}
} // namespace trace2d::assets
