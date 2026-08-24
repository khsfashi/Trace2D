#include "AuthoringFileTransaction.hpp"

#include <trace2d/core/FileTransaction.hpp>

namespace trace2d::agent::detail
{
bool CommitAuthoringTextFile(
    const std::filesystem::path& target,
    const std::string_view contents,
    const std::string_view resourceKind,
    std::string& errorMessage)
{
    return trace2d::core::CommitTextFileAtomically(target, contents, resourceKind, errorMessage);
}
} // namespace trace2d::agent::detail
