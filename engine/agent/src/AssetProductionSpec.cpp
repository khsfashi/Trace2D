#include <trace2d/agent/AssetProductionSpec.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::int64_t AssetProductionFormatVersion = 1;

void AddDiagnostic(
    std::vector<AssetProductionDiagnostic>& diagnostics,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    AssetProductionDiagnostic diagnostic{};
    diagnostic.path = std::move(path);
    diagnostic.message = std::move(message);
    if (node != nullptr)
    {
        const toml::source_position position = node->source().begin;
        diagnostic.line = static_cast<std::size_t>(position.line);
        diagnostic.column = static_cast<std::size_t>(position.column);
    }
    diagnostics.push_back(std::move(diagnostic));
}

bool IsKnownKey(
    const std::string_view key,
    const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<AssetProductionDiagnostic>& diagnostics)
{
    for (const auto& [key, value] : table)
    {
        const std::string_view keyView = key.str();
        if (IsKnownKey(keyView, knownKeys))
        {
            continue;
        }

        std::string fieldPath{path};
        if (!fieldPath.empty())
        {
            fieldPath.push_back('.');
        }
        fieldPath.append(keyView);
        AddDiagnostic(diagnostics, std::move(fieldPath), "Unknown field.", &value);
    }
}

bool ParseToml(
    const std::string_view text,
    const std::string_view sourceName,
    toml::table& root,
    std::vector<AssetProductionDiagnostic>& diagnostics)
{
    try
    {
        root = toml::parse(text, sourceName);
        return true;
    }
    catch (const toml::parse_error& error)
    {
        AssetProductionDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        diagnostics.push_back(std::move(diagnostic));
        return false;
    }
}

const toml::table* ReadRequiredTable(
    const toml::table& parent,
    const std::string_view key,
    const std::string_view path,
    std::vector<AssetProductionDiagnostic>& diagnostics)
{
    const toml::node* node = parent.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required table is missing.");
        return nullptr;
    }

    const toml::table* table = node->as_table();
    if (table == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a table.", node);
    }
    return table;
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<AssetProductionDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value() || value->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a non-empty string.", node);
        return std::nullopt;
    }
    return value;
}

std::optional<std::uint32_t> ReadRequiredPositiveUint32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<AssetProductionDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::int64_t> value = node->value<std::int64_t>();
    if (!value.has_value() || *value <= 0 ||
        static_cast<std::uint64_t>(*value) > std::numeric_limits<std::uint32_t>::max())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a positive 32-bit integer.", node);
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

void ReadStringArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<std::string>& destination,
    std::vector<AssetProductionDiagnostic>& diagnostics,
    const bool required,
    const bool requireNonEmpty)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        if (required)
        {
            AddDiagnostic(diagnostics, std::string{path}, "Required array is missing.");
        }
        return;
    }

    const toml::array* array = node->as_array();
    if (array == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an array of strings.", node);
        return;
    }
    if (requireNonEmpty && array->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected at least one string.", node);
        return;
    }

    destination.reserve(array->size());
    std::unordered_set<std::string> seen{};
    seen.reserve(array->size());
    for (std::size_t index = 0; index < array->size(); ++index)
    {
        const toml::node* item = array->get(index);
        const std::optional<std::string> value =
            item == nullptr ? std::nullopt : item->value<std::string>();
        const std::string itemPath = std::string{path} + "[" + std::to_string(index) + "]";
        if (!value.has_value() || value->empty())
        {
            AddDiagnostic(diagnostics, itemPath, "Expected a non-empty string.", item);
            continue;
        }
        if (!seen.insert(*value).second)
        {
            AddDiagnostic(diagnostics, itemPath, "Duplicate string '" + *value + "'.", item);
            continue;
        }
        destination.push_back(*value);
    }
}

bool IsProjectRelativeReference(const std::string_view value) noexcept
{
    if (value.empty() || value.front() == '/' || value.front() == '\\')
    {
        return false;
    }
    if (value.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
        value[1] == ':')
    {
        return false;
    }
    if (value.find('\\') != std::string_view::npos)
    {
        return false;
    }

    std::size_t begin = 0;
    while (begin <= value.size())
    {
        const std::size_t end = value.find('/', begin);
        const std::size_t count =
            end == std::string_view::npos ? value.size() - begin : end - begin;
        const std::string_view segment = value.substr(begin, count);
        if (segment.empty() || segment == "." || segment == "..")
        {
            return false;
        }
        if (end == std::string_view::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return true;
}

void ValidateProjectReferences(
    const ArtProfile& profile,
    const std::string_view path,
    std::vector<AssetProductionDiagnostic>& diagnostics)
{
    for (std::size_t index = 0; index < profile.approvedReferences.size(); ++index)
    {
        if (!IsProjectRelativeReference(profile.approvedReferences[index]))
        {
            AddDiagnostic(
                diagnostics,
                std::string{path} + ".approved_references[" + std::to_string(index) + "]",
                "Expected a canonical project-relative asset reference using '/' separators.");
        }
    }
}
} // namespace

AssetProductionSpecParseResult ParseAssetProductionSpecToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    AssetProductionSpecParseResult result{};
    toml::table root{};
    if (!ParseToml(text, sourceName, root, result.diagnostics))
    {
        return result;
    }

    ValidateKnownKeys(
        root,
        "",
        {"format_version", "production_set", "items", "art_profiles"},
        result.diagnostics);

    const toml::node* versionNode = root.get("format_version");
    const std::optional<std::int64_t> version =
        versionNode == nullptr ? std::nullopt : versionNode->value<std::int64_t>();
    if (!version.has_value() || *version != AssetProductionFormatVersion)
    {
        AddDiagnostic(
            result.diagnostics,
            "format_version",
            "Expected integer format_version = 1.",
            versionNode);
    }

    AssetProductionSpec spec{};
    if (const toml::table* set =
            ReadRequiredTable(root, "production_set", "production_set", result.diagnostics);
        set != nullptr)
    {
        ValidateKnownKeys(
            *set,
            "production_set",
            {"id", "work_spec", "owner_review_acceptance", "art_profile", "candidates_per_item", "max_provider_calls"},
            result.diagnostics);

        const std::optional<std::string> id =
            ReadRequiredString(*set, "id", "production_set.id", result.diagnostics);
        const std::optional<std::string> workSpec =
            ReadRequiredString(*set, "work_spec", "production_set.work_spec", result.diagnostics);
        const std::optional<std::string> ownerReviewAcceptance = ReadRequiredString(
            *set,
            "owner_review_acceptance",
            "production_set.owner_review_acceptance",
            result.diagnostics);
        const std::optional<std::string> artProfile =
            ReadRequiredString(*set, "art_profile", "production_set.art_profile", result.diagnostics);
        const std::optional<std::uint32_t> candidates = ReadRequiredPositiveUint32(
            *set,
            "candidates_per_item",
            "production_set.candidates_per_item",
            result.diagnostics);
        const std::optional<std::uint32_t> maxProviderCalls = ReadRequiredPositiveUint32(
            *set,
            "max_provider_calls",
            "production_set.max_provider_calls",
            result.diagnostics);

        if (id.has_value()) spec.id = *id;
        if (workSpec.has_value()) spec.workSpecId = *workSpec;
        if (ownerReviewAcceptance.has_value())
        {
            spec.ownerReviewAcceptanceId = *ownerReviewAcceptance;
        }
        if (artProfile.has_value()) spec.artProfileId = *artProfile;
        if (candidates.has_value()) spec.candidatesPerItem = *candidates;
        if (maxProviderCalls.has_value()) spec.maxProviderCalls = *maxProviderCalls;
    }

    std::unordered_set<std::string> itemIds{};
    if (const toml::node* itemsNode = root.get("items"); itemsNode != nullptr)
    {
        const toml::array* items = itemsNode->as_array();
        if (items == nullptr || items->empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "items",
                "Expected at least one production item table.",
                itemsNode);
        }
        else
        {
            spec.items.reserve(items->size());
            itemIds.reserve(items->size());
            for (std::size_t index = 0; index < items->size(); ++index)
            {
                const toml::node* itemNode = items->get(index);
                const toml::table* table =
                    itemNode == nullptr ? nullptr : itemNode->as_table();
                const std::string path = "items[" + std::to_string(index) + "]";
                if (table == nullptr)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        path,
                        "Expected a production item table.",
                        itemNode);
                    continue;
                }

                ValidateKnownKeys(
                    *table,
                    path,
                    {"id", "asset_class", "width", "height", "required_animations", "required_directions", "constraints"},
                    result.diagnostics);

                AssetProductionItem item{};
                const std::optional<std::string> id =
                    ReadRequiredString(*table, "id", path + ".id", result.diagnostics);
                const std::optional<std::string> assetClass = ReadRequiredString(
                    *table,
                    "asset_class",
                    path + ".asset_class",
                    result.diagnostics);
                const std::optional<std::uint32_t> width = ReadRequiredPositiveUint32(
                    *table,
                    "width",
                    path + ".width",
                    result.diagnostics);
                const std::optional<std::uint32_t> height = ReadRequiredPositiveUint32(
                    *table,
                    "height",
                    path + ".height",
                    result.diagnostics);

                if (id.has_value()) item.id = *id;
                if (assetClass.has_value()) item.assetClass = *assetClass;
                if (width.has_value()) item.width = *width;
                if (height.has_value()) item.height = *height;
                ReadStringArray(
                    *table,
                    "required_animations",
                    path + ".required_animations",
                    item.requiredAnimations,
                    result.diagnostics,
                    false,
                    false);
                ReadStringArray(
                    *table,
                    "required_directions",
                    path + ".required_directions",
                    item.requiredDirections,
                    result.diagnostics,
                    false,
                    false);
                ReadStringArray(
                    *table,
                    "constraints",
                    path + ".constraints",
                    item.constraints,
                    result.diagnostics,
                    false,
                    false);

                if (id.has_value() && !itemIds.insert(*id).second)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        path + ".id",
                        "Duplicate item id '" + *id + "'.",
                        table->get("id"));
                }
                spec.items.push_back(std::move(item));
            }
        }
    }
    else
    {
        AddDiagnostic(result.diagnostics, "items", "Required items array is missing.");
    }

    std::unordered_set<std::string> profileIds{};
    if (const toml::node* profilesNode = root.get("art_profiles"); profilesNode != nullptr)
    {
        const toml::array* profiles = profilesNode->as_array();
        if (profiles == nullptr || profiles->empty())
        {
            AddDiagnostic(
                result.diagnostics,
                "art_profiles",
                "Expected at least one Art Profile table.",
                profilesNode);
        }
        else
        {
            spec.artProfiles.reserve(profiles->size());
            profileIds.reserve(profiles->size());
            for (std::size_t index = 0; index < profiles->size(); ++index)
            {
                const toml::node* profileNode = profiles->get(index);
                const toml::table* table =
                    profileNode == nullptr ? nullptr : profileNode->as_table();
                const std::string path = "art_profiles[" + std::to_string(index) + "]";
                if (table == nullptr)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        path,
                        "Expected an Art Profile table.",
                        profileNode);
                    continue;
                }

                ValidateKnownKeys(
                    *table,
                    path,
                    {"id", "description", "creative_constraints", "approved_references"},
                    result.diagnostics);

                ArtProfile profile{};
                const std::optional<std::string> id =
                    ReadRequiredString(*table, "id", path + ".id", result.diagnostics);
                const std::optional<std::string> description = ReadRequiredString(
                    *table,
                    "description",
                    path + ".description",
                    result.diagnostics);
                if (id.has_value()) profile.id = *id;
                if (description.has_value()) profile.description = *description;
                ReadStringArray(
                    *table,
                    "creative_constraints",
                    path + ".creative_constraints",
                    profile.creativeConstraints,
                    result.diagnostics,
                    false,
                    false);
                ReadStringArray(
                    *table,
                    "approved_references",
                    path + ".approved_references",
                    profile.approvedReferences,
                    result.diagnostics,
                    true,
                    true);
                ValidateProjectReferences(profile, path, result.diagnostics);

                if (id.has_value() && !profileIds.insert(*id).second)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        path + ".id",
                        "Duplicate Art Profile id '" + *id + "'.",
                        table->get("id"));
                }
                spec.artProfiles.push_back(std::move(profile));
            }
        }
    }
    else
    {
        AddDiagnostic(
            result.diagnostics,
            "art_profiles",
            "Required art_profiles array is missing.");
    }

    if (!spec.artProfileId.empty() && !profileIds.contains(spec.artProfileId))
    {
        AddDiagnostic(
            result.diagnostics,
            "production_set.art_profile",
            "Production set references unknown Art Profile '" + spec.artProfileId + "'.");
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    result.spec = std::move(spec);
    return result;
}

std::vector<AssetProductionDiagnostic> ValidateAssetProductionSpecAgainstWorkSpec(
    const AssetProductionSpec& spec,
    const WorkSpec& workSpec)
{
    std::vector<AssetProductionDiagnostic> diagnostics{};

    if (spec.workSpecId != workSpec.id)
    {
        AddDiagnostic(
            diagnostics,
            "production_set.work_spec",
            "Production set references WorkSpec '" + spec.workSpecId +
                "' but supplied WorkSpec id is '" + workSpec.id + "'.");
    }

    const auto reviewAcceptance = std::find_if(
        workSpec.acceptance.begin(),
        workSpec.acceptance.end(),
        [&spec](const AcceptanceCriterion& criterion)
        {
            return criterion.id == spec.ownerReviewAcceptanceId;
        });
    if (reviewAcceptance == workSpec.acceptance.end())
    {
        AddDiagnostic(
            diagnostics,
            "production_set.owner_review_acceptance",
            "Production set references unknown WorkSpec acceptance '" +
                spec.ownerReviewAcceptanceId + "'.");
    }
    else if (reviewAcceptance->verification != VerificationClass::Human)
    {
        AddDiagnostic(
            diagnostics,
            "production_set.owner_review_acceptance",
            "Owner review must reference a WorkSpec acceptance with verification = human.");
    }

    return diagnostics;
}
} // namespace trace2d::agent
