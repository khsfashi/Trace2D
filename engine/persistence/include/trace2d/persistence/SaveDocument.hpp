#pragma once

#include <trace2d/scene/Components.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::persistence
{
inline constexpr std::uint32_t CurrentSaveFormatVersion = 1U;
inline constexpr std::string_view SaveSchemaId = "trace2d.save.v1";

struct SaveDiagnostic final
{
    std::string source{};
    std::string path{};
    std::string message{};
    std::size_t line{0U};
    std::size_t column{0U};
};

struct SaveRecord final
{
    std::string id{};
    std::string typeId{};
    std::uint32_t schemaVersion{0U};
    scene::ComponentAuthoringObject data{};
};

struct SaveDocument final
{
    std::string saveId{};
    std::string producerVersion{};
    std::vector<SaveRecord> records{};
};

struct SaveParseResult final
{
    std::optional<SaveDocument> document{};
    std::vector<SaveDiagnostic> diagnostics{};
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

struct SaveSerializeResult final
{
    std::string text{};
    std::vector<SaveDiagnostic> diagnostics{};
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

struct SaveWriteResult final
{
    std::vector<SaveDiagnostic> diagnostics{};
    [[nodiscard]] bool Succeeded() const noexcept { return diagnostics.empty(); }
};

[[nodiscard]] SaveParseResult ParseSaveDocumentJson(
    std::string_view text,
    std::string_view sourceName = {});

[[nodiscard]] SaveSerializeResult SerializeSaveDocumentJson(const SaveDocument& document);

[[nodiscard]] SaveParseResult ReadSaveDocumentFile(const std::filesystem::path& path);

[[nodiscard]] SaveWriteResult WriteSaveDocumentFile(
    const std::filesystem::path& path,
    const SaveDocument& document);
} // namespace trace2d::persistence
