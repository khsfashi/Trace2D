#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
enum class WorkItemState : std::uint8_t
{
    Requested = 0,
    Planned,
    Implemented,
    Verified,
    ReviewNeeded,
    Approved,
    Failed,
};

[[nodiscard]] std::string_view ToString(WorkItemState state) noexcept;

enum class VerificationClass : std::uint8_t
{
    Deterministic = 0,
    Presentation,
    Multimodal,
    Human,
};

[[nodiscard]] std::string_view ToString(VerificationClass value) noexcept;

enum class CapabilityMinimum : std::uint8_t
{
    Available = 0,
    Tested,
    ProductionSupported,
};

[[nodiscard]] std::string_view ToString(CapabilityMinimum value) noexcept;

enum class ExternalTruthKind : std::uint8_t
{
    GitHub = 0,
    Ci,
    Environment,
    Hardware,
    License,
    HumanApproval,
};

[[nodiscard]] std::string_view ToString(ExternalTruthKind value) noexcept;

enum class LocalReadiness : std::uint8_t
{
    Ready = 0,
    Blocked,
    ReviewNeeded,
    Complete,
    Failed,
};

[[nodiscard]] std::string_view ToString(LocalReadiness value) noexcept;

struct WorkSpecDiagnostic final
{
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct WorkDeliverable final
{
    std::string id{};
    std::string description{};
    WorkItemState state{WorkItemState::Requested};
};

struct CapabilityRequirement final
{
    std::string deliverableId{};
    std::string capabilityId{};
    CapabilityMinimum minimum{CapabilityMinimum::Available};
};

struct AcceptanceCriterion final
{
    std::string id{};
    std::string deliverableId{};
    std::string description{};
    VerificationClass verification{VerificationClass::Deterministic};
    WorkItemState state{WorkItemState::Requested};
};

struct ExternalTruthRequirement final
{
    std::string id{};
    ExternalTruthKind kind{ExternalTruthKind::GitHub};
    std::string description{};
};

struct WorkSpec final
{
    std::string id{};
    std::string intent{};
    WorkItemState state{WorkItemState::Requested};
    std::vector<std::string> constraints{};
    std::vector<WorkDeliverable> deliverables{};
    std::vector<CapabilityRequirement> capabilityRequirements{};
    std::vector<AcceptanceCriterion> acceptance{};
    std::vector<ExternalTruthRequirement> externalTruth{};
};

struct CapabilityDeclaration final
{
    std::string id{};
    bool available{false};
    bool tested{false};
    bool productionSupported{false};
    bool deterministicVerification{false};
    bool presentationEvidence{false};
    bool hardwareEvidence{false};
    bool humanJudgment{false};
    std::vector<std::string> evidence{};
};

struct CapabilityCatalog final
{
    std::vector<CapabilityDeclaration> capabilities{};
};

struct WorkSpecParseResult final
{
    std::optional<WorkSpec> spec{};
    std::vector<WorkSpecDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return spec.has_value() && diagnostics.empty();
    }
};

struct CapabilityCatalogParseResult final
{
    std::optional<CapabilityCatalog> catalog{};
    std::vector<WorkSpecDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return catalog.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] WorkSpecParseResult ParseWorkSpecToml(
    std::string_view text,
    std::string_view sourceName = {});

[[nodiscard]] CapabilityCatalogParseResult ParseCapabilityCatalogToml(
    std::string_view text,
    std::string_view sourceName = {});

struct CapabilityEvaluation final
{
    std::string deliverableId{};
    std::string capabilityId{};
    CapabilityMinimum minimum{CapabilityMinimum::Available};
    bool declared{false};
    bool available{false};
    bool tested{false};
    bool productionSupported{false};
    bool deterministicVerification{false};
    bool presentationEvidence{false};
    bool hardwareEvidence{false};
    bool humanJudgment{false};
    bool eligible{false};
    std::string reason{};
};

struct WorkEvaluation final
{
    LocalReadiness localReadiness{LocalReadiness::Ready};
    std::vector<CapabilityEvaluation> capabilityRequirements{};
    std::vector<std::string> outstandingAcceptanceIds{};
    std::vector<std::string> reviewAcceptanceIds{};
    std::vector<ExternalTruthRequirement> externalTruth{};

    [[nodiscard]] bool RequiresLiveTruth() const noexcept
    {
        return !externalTruth.empty();
    }
};

[[nodiscard]] WorkEvaluation EvaluateWork(
    const WorkSpec& spec,
    const CapabilityCatalog& catalog);
} // namespace trace2d::agent
