#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace trace2d::agent::detail
{
// Commits complete validated text through a unique sibling temporary file.
// The target is replaced only after the temporary file has been written and closed.
[[nodiscard]] bool CommitAuthoringTextFile(
    const std::filesystem::path& target,
    std::string_view contents,
    std::string_view resourceKind,
    std::string& errorMessage);
} // namespace trace2d::agent::detail
