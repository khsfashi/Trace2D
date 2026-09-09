#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace trace2d::core
{
using TemporaryTextFileValidator = std::function<bool(const std::filesystem::path&, std::string&)>;

// Commits complete text through a unique sibling temporary file. The target is
// replaced only after the temporary file has been fully written, closed, and
// accepted by the optional validator. This is explicit tooling/lifecycle I/O;
// it is not intended for simulation or rendering hot paths.
[[nodiscard]] bool CommitTextFileAtomically(
    const std::filesystem::path& target,
    std::string_view contents,
    std::string_view resourceKind,
    std::string& errorMessage,
    const TemporaryTextFileValidator& validateTemporary = {});
} // namespace trace2d::core
