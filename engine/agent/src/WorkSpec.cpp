#include <trace2d/agent/WorkSpec.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::int64_t WorkSpecFormatVersion = 1;
constexpr std::int64_t CapabilityCatalogFormatVersion = 1;

void AddDiagnostic(
    std::vector<WorkSpecDiagnostic>& diagnostics,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    WorkSpecDiagnostic diagnostic{};
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
    std::vector<WorkSpecDiagnostic>& diagnostics)
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

const toml::table* ReadRequiredTable(
    const toml::table& parent,
    const std::string_view key,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
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
    std::vector<WorkSpecDiagnostic>& diagnostics)
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

std::optional<bool> ReadRequiredBool(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<bool> value = node->value<bool>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a boolean.", node);
    }
    return value;
}

bool ReadStringArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<std::string>& destination,
    std::vector<WorkSpecDiagnostic>& diagnostics,
    const bool required)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        if (required)
        {
            AddDiagnostic(diagnostics, std::string{path}, "Required array is missing.");
            return false;
        }
        return true;
    }

    const toml::array* array = node->as_array();
    if (array == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an array of strings.", node);
        return false;
    }

    destination.reserve(array->size());
    for (std::size_t index = 0; index < array->size(); ++index)
    {
        const toml::node* item = array->get(index);
        const std::optional<std::string> value =
            item == nullptr ? std::nullopt : item->value<std::string>();
        if (!value.has_value() || value->empty())
        {
            AddDiagnostic(
                diagnostics,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected a non-empty string.",
                item);
            continue;
        }
        destination.push_back(*value);
    }
    return true;
}

std::optional<WorkItemState> ParseWorkItemState(
    const toml::node* node,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a work-state string.", node);
        return std::nullopt;
    }

    if (*value == "requested") return WorkItemState::Requested;
    if (*value == "planned") return WorkItemState::Planned;
    if (*value == "implemented") return WorkItemState::Implemented;
    if (*value == "verified") return WorkItemState::Verified;
    if (*value == "review_needed") return WorkItemState::ReviewNeeded;
    if (*value == "approved") return WorkItemState::Approved;
    if (*value == "failed") return WorkItemState::Failed;

    AddDiagnostic(
        diagnostics,
        std::string{path},
        "Supported states are requested, planned, implemented, verified, review_needed, approved, and failed.",
        node);
    return std::nullopt;
}

std::optional<VerificationClass> ParseVerificationClass(
    const toml::node* node,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a verification-class string.", node);
        return std::nullopt;
    }

    if (*value == "deterministic") return VerificationClass::Deterministic;
    if (*value == "presentation") return VerificationClass::Presentation;
    if (*value == "multimodal") return VerificationClass::Multimodal;
    if (*value == "human") return VerificationClass::Human;

    AddDiagnostic(
        diagnostics,
        std::string{path},
        "Supported verification classes are deterministic, presentation, multimodal, and human.",
        node);
    return std::nullopt;
}

std::optional<CapabilityMinimum> ParseCapabilityMinimum(
    const toml::node* node,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a capability-minimum string.", node);
        return std::nullopt;
    }

    if (*value == "available") return CapabilityMinimum::Available;
    if (*value == "tested") return CapabilityMinimum::Tested;
    if (*value == "production_supported") return CapabilityMinimum::ProductionSupported;

    AddDiagnostic(
        diagnostics,
        std::string{path},
        "Supported capability minimums are available, tested, and production_supported.",
        node);
    return std::nullopt;
}

std::optional<ExternalTruthKind> ParseExternalTruthKind(
    const toml::node* node,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an external-truth kind string.", node);
        return std::nullopt;
    }

    if (*value == "github") return ExternalTruthKind::GitHub;
    if (*value == "ci") return ExternalTruthKind::Ci;
    if (*value == "environment") return ExternalTruthKind::Environment;
    if (*value == "hardware") return ExternalTruthKind::Hardware;
    if (*value == "license") return ExternalTruthKind::License;
    if (*value == "human_approval") return ExternalTruthKind::HumanApproval;

    AddDiagnostic(
        diagnostics,
        std::string{path},
        "Supported external-truth kinds are github, ci, environment, hardware, license, and human_approval.",
        node);
    return std::nullopt;
}

bool ValidateFormatVersion(
    const toml::table& root,
    const std::int64_t expected,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* node = root.get("format_version");
    const std::optional<std::int64_t> version =
        node == nullptr ? std::nullopt : node->value<std::int64_t>();
    if (!version.has_value() || *version != expected)
    {
        AddDiagnostic(
            diagnostics,
            "format_version",
            "Expected integer format_version = " + std::to_string(expected) + ".",
            node);
        return false;
    }
    return true;
}

bool ParseToml(
    const std::string_view text,
    const std::string_view sourceName,
    toml::table& root,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    try
    {
        root = toml::parse(text, sourceName);
        return true;
    }
    catch (const toml::parse_error& error)
    {
        WorkSpecDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        diagnostics.push_back(std::move(diagnostic));
        return false;
    }
}

template <typename Item>
bool InsertUniqueId(
    std::unordered_set<std::string>& ids,
    const Item& item,
    const std::string_view path,
    const std::size_t index,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    if (ids.insert(item.id).second)
    {
        return true;
    }

    AddDiagnostic(
        diagnostics,
        std::string{path} + "[" + std::to_string(index) + "].id",
        "Duplicate id '" + item.id + "'.");
    return false;
}

bool AcceptanceIsComplete(const AcceptanceCriterion& criterion) noexcept
{
    if (criterion.state == WorkItemState::Approved)
    {
        return true;
    }

    if (criterion.verification == VerificationClass::Deterministic ||
        criterion.verification == VerificationClass::Presentation)
    {
        return criterion.state == WorkItemState::Verified;
    }
    return false;
}

bool AcceptanceNeedsReview(const AcceptanceCriterion& criterion) noexcept
{
    if (criterion.state == WorkItemState::ReviewNeeded)
    {
        return true;
    }

    return (criterion.verification == VerificationClass::Multimodal ||
            criterion.verification == VerificationClass::Human) &&
        criterion.state == WorkItemState::Verified;
}
} // namespace

std::string_view ToString(const WorkItemState state) noexcept
{
    switch (state)
    {
    case WorkItemState::Requested: return "requested";
    case WorkItemState::Planned: return "planned";
    case WorkItemState::Implemented: return "implemented";
    case WorkItemState::Verified: return "verified";
    case WorkItemState::ReviewNeeded: return "review_needed";
    case WorkItemState::Approved: return "approved";
    case WorkItemState::Failed: return "failed";
    }
    return "unknown";
}

std::string_view ToString(const VerificationClass value) noexcept
{
    switch (value)
    {
    case VerificationClass::Deterministic: return "deterministic";
    case VerificationClass::Presentation: return "presentation";
    case VerificationClass::Multimodal: return "multimodal";
    case VerificationClass::Human: return "human";
    }
    return "unknown";
}

std::string_view ToString(const CapabilityMinimum value) noexcept
{
    switch (value)
    {
    case CapabilityMinimum::Available: return "available";
    case CapabilityMinimum::Tested: return "tested";
    case CapabilityMinimum::ProductionSupported: return "production_supported";
    }
    return "unknown";
}

std::string_view ToString(const ExternalTruthKind value) noexcept
{
    switch (value)
    {
    case ExternalTruthKind::GitHub: return "github";
    case ExternalTruthKind::Ci: return "ci";
    case ExternalTruthKind::Environment: return "environment";
    case ExternalTruthKind::Hardware: return "hardware";
    case ExternalTruthKind::License: return "license";
    case ExternalTruthKind::HumanApproval: return "human_approval";
    }
    return "unknown";
}

std::string_view ToString(const LocalReadiness value) noexcept
{
    switch (value)
    {
    case LocalReadiness::Ready: return "ready";
    case LocalReadiness::Blocked: return "blocked";
    case LocalReadiness::ReviewNeeded: return "review_needed";
    case LocalReadiness::Complete: return "complete";
    case LocalReadiness::Failed: return "failed";
    }
    return "unknown";
}

WorkSpecParseResult ParseWorkSpecToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    WorkSpecParseResult result{};
    toml::table root{};
    if (!ParseToml(text, sourceName, root, result.diagnostics))
    {
        return result;
    }

    ValidateKnownKeys(
        root,
        "",
        {"format_version", "work", "deliverables", "requirements", "acceptance", "external_truth"},
        result.diagnostics);
    ValidateFormatVersion(root, WorkSpecFormatVersion, result.diagnostics);

    WorkSpec spec{};
    if (const toml::table* work = ReadRequiredTable(root, "work", "work", result.diagnostics);
        work != nullptr)
    {
        ValidateKnownKeys(*work, "work", {"id", "intent", "state", "constraints"}, result.diagnostics);
        const std::optional<std::string> id =
            ReadRequiredString(*work, "id", "work.id", result.diagnostics);
        const std::optional<std::string> intent =
            ReadRequiredString(*work, "intent", "work.intent", result.diagnostics);
        const std::optional<WorkItemState> state =
            ParseWorkItemState(work->get("state"), "work.state", result.diagnostics);
        static_cast<void>(
            ReadStringArray(*work, "constraints", "work.constraints", spec.constraints, result.diagnostics, false));

        if (id.has_value()) spec.id = *id;
        if (intent.has_value()) spec.intent = *intent;
        if (state.has_value()) spec.state = *state;
    }

    std::unordered_set<std::string> deliverableIds{};
    if (const toml::node* node = root.get("deliverables"); node != nullptr)
    {
        const toml::array* array = node->as_array();
        if (array == nullptr || array->empty())
        {
            AddDiagnostic(result.diagnostics, "deliverables", "Expected at least one deliverable table.", node);
        }
        else
        {
            spec.deliverables.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const toml::node* itemNode = array->get(index);
                const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
                const std::string path = "deliverables[" + std::to_string(index) + "]";
                if (item == nullptr)
                {
                    AddDiagnostic(result.diagnostics, path, "Expected a deliverable table.", itemNode);
                    continue;
                }

                ValidateKnownKeys(*item, path, {"id", "description", "state"}, result.diagnostics);
                WorkDeliverable deliverable{};
                const std::optional<std::string> id =
                    ReadRequiredString(*item, "id", path + ".id", result.diagnostics);
                const std::optional<std::string> description =
                    ReadRequiredString(*item, "description", path + ".description", result.diagnostics);
                const std::optional<WorkItemState> state =
                    ParseWorkItemState(item->get("state"), path + ".state", result.diagnostics);
                if (id.has_value()) deliverable.id = *id;
                if (description.has_value()) deliverable.description = *description;
                if (state.has_value()) deliverable.state = *state;

                if (id.has_value())
                {
                    static_cast<void>(
                        InsertUniqueId(deliverableIds, deliverable, "deliverables", index, result.diagnostics));
                }
                spec.deliverables.push_back(std::move(deliverable));
            }
        }
    }
    else
    {
        AddDiagnostic(result.diagnostics, "deliverables", "Required deliverables array is missing.");
    }

    if (const toml::node* node = root.get("requirements"); node != nullptr)
    {
        const toml::array* array = node->as_array();
        if (array == nullptr)
        {
            AddDiagnostic(result.diagnostics, "requirements", "Expected an array of requirement tables.", node);
        }
        else
        {
            spec.capabilityRequirements.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const toml::node* itemNode = array->get(index);
                const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
                const std::string path = "requirements[" + std::to_string(index) + "]";
                if (item == nullptr)
                {
                    AddDiagnostic(result.diagnostics, path, "Expected a requirement table.", itemNode);
                    continue;
                }

                ValidateKnownKeys(*item, path, {"deliverable", "capability", "minimum"}, result.diagnostics);
                CapabilityRequirement requirement{};
                const std::optional<std::string> deliverable =
                    ReadRequiredString(*item, "deliverable", path + ".deliverable", result.diagnostics);
                const std::optional<std::string> capability =
                    ReadRequiredString(*item, "capability", path + ".capability", result.diagnostics);
                const std::optional<CapabilityMinimum> minimum =
                    ParseCapabilityMinimum(item->get("minimum"), path + ".minimum", result.diagnostics);
                if (deliverable.has_value()) requirement.deliverableId = *deliverable;
                if (capability.has_value()) requirement.capabilityId = *capability;
                if (minimum.has_value()) requirement.minimum = *minimum;

                if (deliverable.has_value() && !deliverableIds.contains(*deliverable))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        path + ".deliverable",
                        "Requirement references unknown deliverable '" + *deliverable + "'.",
                        item->get("deliverable"));
                }
                spec.capabilityRequirements.push_back(std::move(requirement));
            }
        }
    }

    std::unordered_set<std::string> acceptanceIds{};
    if (const toml::node* node = root.get("acceptance"); node != nullptr)
    {
        const toml::array* array = node->as_array();
        if (array == nullptr || array->empty())
        {
            AddDiagnostic(result.diagnostics, "acceptance", "Expected at least one acceptance table.", node);
        }
        else
        {
            spec.acceptance.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const toml::node* itemNode = array->get(index);
                const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
                const std::string path = "acceptance[" + std::to_string(index) + "]";
                if (item == nullptr)
                {
                    AddDiagnostic(result.diagnostics, path, "Expected an acceptance table.", itemNode);
                    continue;
                }

                ValidateKnownKeys(
                    *item,
                    path,
                    {"id", "deliverable", "description", "verification", "state"},
                    result.diagnostics);
                AcceptanceCriterion criterion{};
                const std::optional<std::string> id =
                    ReadRequiredString(*item, "id", path + ".id", result.diagnostics);
                const std::optional<std::string> deliverable =
                    ReadRequiredString(*item, "deliverable", path + ".deliverable", result.diagnostics);
                const std::optional<std::string> description =
                    ReadRequiredString(*item, "description", path + ".description", result.diagnostics);
                const std::optional<VerificationClass> verification =
                    ParseVerificationClass(item->get("verification"), path + ".verification", result.diagnostics);
                const std::optional<WorkItemState> state =
                    ParseWorkItemState(item->get("state"), path + ".state", result.diagnostics);

                if (id.has_value()) criterion.id = *id;
                if (deliverable.has_value()) criterion.deliverableId = *deliverable;
                if (description.has_value()) criterion.description = *description;
                if (verification.has_value()) criterion.verification = *verification;
                if (state.has_value()) criterion.state = *state;

                if (id.has_value())
                {
                    static_cast<void>(
                        InsertUniqueId(acceptanceIds, criterion, "acceptance", index, result.diagnostics));
                }
                if (deliverable.has_value() && !deliverableIds.contains(*deliverable))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        path + ".deliverable",
                        "Acceptance criterion references unknown deliverable '" + *deliverable + "'.",
                        item->get("deliverable"));
                }
                spec.acceptance.push_back(std::move(criterion));
            }
        }
    }
    else
    {
        AddDiagnostic(result.diagnostics, "acceptance", "Required acceptance array is missing.");
    }

    std::unordered_set<std::string> externalTruthIds{};
    if (const toml::node* node = root.get("external_truth"); node != nullptr)
    {
        const toml::array* array = node->as_array();
        if (array == nullptr)
        {
            AddDiagnostic(result.diagnostics, "external_truth", "Expected an array of external-truth tables.", node);
        }
        else
        {
            spec.externalTruth.reserve(array->size());
            for (std::size_t index = 0; index < array->size(); ++index)
            {
                const toml::node* itemNode = array->get(index);
                const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
                const std::string path = "external_truth[" + std::to_string(index) + "]";
                if (item == nullptr)
                {
                    AddDiagnostic(result.diagnostics, path, "Expected an external-truth table.", itemNode);
                    continue;
                }

                ValidateKnownKeys(*item, path, {"id", "kind", "description"}, result.diagnostics);
                ExternalTruthRequirement requirement{};
                const std::optional<std::string> id =
                    ReadRequiredString(*item, "id", path + ".id", result.diagnostics);
                const std::optional<ExternalTruthKind> kind =
                    ParseExternalTruthKind(item->get("kind"), path + ".kind", result.diagnostics);
                const std::optional<std::string> description =
                    ReadRequiredString(*item, "description", path + ".description", result.diagnostics);

                if (id.has_value()) requirement.id = *id;
                if (kind.has_value()) requirement.kind = *kind;
                if (description.has_value()) requirement.description = *description;

                if (id.has_value())
                {
                    static_cast<void>(
                        InsertUniqueId(externalTruthIds, requirement, "external_truth", index, result.diagnostics));
                }
                spec.externalTruth.push_back(std::move(requirement));
            }
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    result.spec = std::move(spec);
    return result;
}

CapabilityCatalogParseResult ParseCapabilityCatalogToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    CapabilityCatalogParseResult result{};
    toml::table root{};
    if (!ParseToml(text, sourceName, root, result.diagnostics))
    {
        return result;
    }

    ValidateKnownKeys(root, "", {"format_version", "capabilities"}, result.diagnostics);
    ValidateFormatVersion(root, CapabilityCatalogFormatVersion, result.diagnostics);

    CapabilityCatalog catalog{};
    std::unordered_set<std::string> capabilityIds{};
    const toml::node* node = root.get("capabilities");
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->empty())
    {
        AddDiagnostic(result.diagnostics, "capabilities", "Expected at least one capability table.", node);
    }
    else
    {
        catalog.capabilities.reserve(array->size());
        for (std::size_t index = 0; index < array->size(); ++index)
        {
            const toml::node* itemNode = array->get(index);
            const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
            const std::string path = "capabilities[" + std::to_string(index) + "]";
            if (item == nullptr)
            {
                AddDiagnostic(result.diagnostics, path, "Expected a capability table.", itemNode);
                continue;
            }

            ValidateKnownKeys(
                *item,
                path,
                {"id", "available", "tested", "production_supported", "deterministic_verification",
                 "presentation_evidence", "hardware_evidence", "human_judgment", "evidence"},
                result.diagnostics);

            CapabilityDeclaration capability{};
            const std::optional<std::string> id =
                ReadRequiredString(*item, "id", path + ".id", result.diagnostics);
            const std::optional<bool> available =
                ReadRequiredBool(*item, "available", path + ".available", result.diagnostics);
            const std::optional<bool> tested =
                ReadRequiredBool(*item, "tested", path + ".tested", result.diagnostics);
            const std::optional<bool> productionSupported =
                ReadRequiredBool(
                    *item,
                    "production_supported",
                    path + ".production_supported",
                    result.diagnostics);
            const std::optional<bool> deterministicVerification =
                ReadRequiredBool(
                    *item,
                    "deterministic_verification",
                    path + ".deterministic_verification",
                    result.diagnostics);
            const std::optional<bool> presentationEvidence =
                ReadRequiredBool(
                    *item,
                    "presentation_evidence",
                    path + ".presentation_evidence",
                    result.diagnostics);
            const std::optional<bool> hardwareEvidence =
                ReadRequiredBool(
                    *item,
                    "hardware_evidence",
                    path + ".hardware_evidence",
                    result.diagnostics);
            const std::optional<bool> humanJudgment =
                ReadRequiredBool(
                    *item,
                    "human_judgment",
                    path + ".human_judgment",
                    result.diagnostics);

            if (id.has_value()) capability.id = *id;
            if (available.has_value()) capability.available = *available;
            if (tested.has_value()) capability.tested = *tested;
            if (productionSupported.has_value()) capability.productionSupported = *productionSupported;
            if (deterministicVerification.has_value())
            {
                capability.deterministicVerification = *deterministicVerification;
            }
            if (presentationEvidence.has_value()) capability.presentationEvidence = *presentationEvidence;
            if (hardwareEvidence.has_value()) capability.hardwareEvidence = *hardwareEvidence;
            if (humanJudgment.has_value()) capability.humanJudgment = *humanJudgment;
            static_cast<void>(
                ReadStringArray(*item, "evidence", path + ".evidence", capability.evidence, result.diagnostics, true));

            if (id.has_value())
            {
                static_cast<void>(
                    InsertUniqueId(capabilityIds, capability, "capabilities", index, result.diagnostics));
            }

            if (capability.tested && !capability.available)
            {
                AddDiagnostic(
                    result.diagnostics,
                    path + ".tested",
                    "A tested capability must also be available.",
                    item->get("tested"));
            }
            if (capability.productionSupported && !capability.tested)
            {
                AddDiagnostic(
                    result.diagnostics,
                    path + ".production_supported",
                    "A production-supported capability must also be tested.",
                    item->get("production_supported"));
            }
            if ((capability.available || capability.tested || capability.productionSupported) &&
                capability.evidence.empty())
            {
                AddDiagnostic(
                    result.diagnostics,
                    path + ".evidence",
                    "Available/tested/supported capability claims require at least one versioned evidence reference.",
                    item->get("evidence"));
            }

            catalog.capabilities.push_back(std::move(capability));
        }
    }

    if (!result.diagnostics.empty())
    {
        return result;
    }

    result.catalog = std::move(catalog);
    return result;
}

WorkEvaluation EvaluateWork(
    const WorkSpec& spec,
    const CapabilityCatalog& catalog)
{
    WorkEvaluation evaluation{};
    evaluation.externalTruth = spec.externalTruth;

    std::unordered_map<std::string_view, const CapabilityDeclaration*> declarations{};
    declarations.reserve(catalog.capabilities.size());
    for (const CapabilityDeclaration& declaration : catalog.capabilities)
    {
        declarations.emplace(declaration.id, &declaration);
    }

    bool blocked = false;
    evaluation.capabilityRequirements.reserve(spec.capabilityRequirements.size());
    for (const CapabilityRequirement& requirement : spec.capabilityRequirements)
    {
        CapabilityEvaluation capability{};
        capability.deliverableId = requirement.deliverableId;
        capability.capabilityId = requirement.capabilityId;
        capability.minimum = requirement.minimum;

        const auto found = declarations.find(requirement.capabilityId);
        if (found == declarations.end())
        {
            capability.reason = "missing_capability_declaration";
            blocked = true;
            evaluation.capabilityRequirements.push_back(std::move(capability));
            continue;
        }

        const CapabilityDeclaration& declaration = *found->second;
        capability.declared = true;
        capability.available = declaration.available;
        capability.tested = declaration.tested;
        capability.productionSupported = declaration.productionSupported;
        capability.deterministicVerification = declaration.deterministicVerification;
        capability.presentationEvidence = declaration.presentationEvidence;
        capability.hardwareEvidence = declaration.hardwareEvidence;
        capability.humanJudgment = declaration.humanJudgment;

        bool eligible = declaration.available;
        std::string reason = eligible ? "eligible" : "unavailable";
        if (eligible && requirement.minimum == CapabilityMinimum::Tested && !declaration.tested)
        {
            eligible = false;
            reason = "not_tested";
        }
        if (eligible &&
            requirement.minimum == CapabilityMinimum::ProductionSupported &&
            !declaration.productionSupported)
        {
            eligible = false;
            reason = "not_production_supported";
        }

        capability.eligible = eligible;
        capability.reason = std::move(reason);
        blocked = blocked || !eligible;
        evaluation.capabilityRequirements.push_back(std::move(capability));
    }

    bool failed = spec.state == WorkItemState::Failed;
    bool allAcceptanceComplete = !spec.acceptance.empty();
    bool reviewNeeded = false;
    for (const WorkDeliverable& deliverable : spec.deliverables)
    {
        failed = failed || deliverable.state == WorkItemState::Failed;
    }

    for (const AcceptanceCriterion& criterion : spec.acceptance)
    {
        if (criterion.state == WorkItemState::Failed)
        {
            failed = true;
        }

        if (!AcceptanceIsComplete(criterion))
        {
            allAcceptanceComplete = false;
            evaluation.outstandingAcceptanceIds.push_back(criterion.id);
        }

        if (AcceptanceNeedsReview(criterion))
        {
            reviewNeeded = true;
            evaluation.reviewAcceptanceIds.push_back(criterion.id);
        }
    }

    if (failed)
    {
        evaluation.localReadiness = LocalReadiness::Failed;
    }
    else if (blocked)
    {
        evaluation.localReadiness = LocalReadiness::Blocked;
    }
    else if (allAcceptanceComplete)
    {
        evaluation.localReadiness = LocalReadiness::Complete;
    }
    else if (reviewNeeded)
    {
        evaluation.localReadiness = LocalReadiness::ReviewNeeded;
    }
    else
    {
        evaluation.localReadiness = LocalReadiness::Ready;
    }

    return evaluation;
}
} // namespace trace2d::agent
