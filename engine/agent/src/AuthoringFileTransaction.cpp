#include "AuthoringFileTransaction.hpp"

#include <atomic>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace trace2d::agent::detail
{
namespace
{
constexpr std::uint32_t MaximumTemporaryPathAttempts = 8U;
std::atomic<std::uint64_t> TemporarySerial{0U};

std::uint64_t ProcessId() noexcept
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::optional<std::filesystem::path> MakeTemporaryPath(
    const std::filesystem::path& target,
    const std::string_view resourceKind,
    std::string& errorMessage)
{
    for (std::uint32_t attempt = 0U; attempt < MaximumTemporaryPathAttempts; ++attempt)
    {
        std::filesystem::path candidate = target;
        candidate += ".trace2d-authoring." + std::to_string(ProcessId()) + "." +
            std::to_string(TemporarySerial.fetch_add(1U, std::memory_order_relaxed)) + ".tmp";

        std::error_code error{};
        const bool exists = std::filesystem::exists(candidate, error);
        if (error)
        {
            errorMessage = "Unable to inspect temporary " + std::string{resourceKind} +
                " authoring path: " + error.message();
            return std::nullopt;
        }
        if (!exists)
        {
            return candidate;
        }
    }

    errorMessage = "Unable to reserve a unique sibling temporary path for " +
        std::string{resourceKind} + " authoring.";
    return std::nullopt;
}

bool WriteTemporaryFile(
    const std::filesystem::path& path,
    const std::string_view contents,
    const std::string_view resourceKind,
    std::string& errorMessage)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        errorMessage = "Unable to open temporary " + std::string{resourceKind} +
            " resource for writing.";
        return false;
    }

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    if (!output)
    {
        errorMessage = "Unable to write temporary " + std::string{resourceKind} +
            " resource completely.";
        output.close();
        return false;
    }

    output.close();
    if (!output)
    {
        errorMessage = "Unable to close temporary " + std::string{resourceKind} +
            " resource after writing.";
        return false;
    }
    return true;
}

bool ReplaceFileAtomically(
    const std::filesystem::path& source,
    const std::filesystem::path& target,
    const std::string_view resourceKind,
    std::string& errorMessage)
{
#ifdef _WIN32
    if (::MoveFileExW(
            source.c_str(),
            target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
    {
        errorMessage = "Unable to atomically replace " + std::string{resourceKind} +
            " resource: " +
            std::system_category().message(static_cast<int>(::GetLastError()));
        return false;
    }
    return true;
#else
    std::error_code error{};
    std::filesystem::rename(source, target, error);
    if (error)
    {
        errorMessage = "Unable to atomically replace " + std::string{resourceKind} +
            " resource: " + error.message();
        return false;
    }
    return true;
#endif
}

void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
{
    std::error_code ignored{};
    static_cast<void>(std::filesystem::remove(path, ignored));
}
} // namespace

bool CommitAuthoringTextFile(
    const std::filesystem::path& target,
    const std::string_view contents,
    const std::string_view resourceKind,
    std::string& errorMessage)
{
    const std::optional<std::filesystem::path> temporary =
        MakeTemporaryPath(target, resourceKind, errorMessage);
    if (!temporary.has_value())
    {
        return false;
    }

    if (!WriteTemporaryFile(*temporary, contents, resourceKind, errorMessage))
    {
        RemoveTemporaryFile(*temporary);
        return false;
    }
    if (!ReplaceFileAtomically(*temporary, target, resourceKind, errorMessage))
    {
        RemoveTemporaryFile(*temporary);
        return false;
    }
    return true;
}
} // namespace trace2d::agent::detail
