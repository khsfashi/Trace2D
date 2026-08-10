#include <trace2d/agent/WorkSpec.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace
{
constexpr std::string_view CapabilityCatalog = R"toml(
format_version = 1

[[capabilities]]
id = "particles.cpu_reference"
available = true
tested = true
production_supported = false
deterministic_verification = true
presentation_evidence = true
hardware_evidence = false
human_judgment = false
evidence = ["tests/particles/ParticleReferenceTests.cpp"]

[[capabilities]]
id = "physics2d"
available = false
tested = false
production_supported = false
deterministic_verification = false
presentation_evidence = false
hardware_evidence = false
human_judgment = false
evidence = ["docs/PRODUCTION_GAPS.md"]
)toml";

TEST(WorkSpecTests, DerivesReviewNeededWithoutClearingHumanApproval)
{
    constexpr std::string_view specText = R"toml(
format_version = 1

[work]
id = "particle-demo"
intent = "Author and verify one representative particle effect."
state = "implemented"
constraints = ["Keep backend choice explicit."]

[[deliverables]]
id = "effect"
description = "Representative effect"
state = "implemented"

[[requirements]]
deliverable = "effect"
capability = "particles.cpu_reference"
minimum = "tested"

[[acceptance]]
id = "semantic-proof"
deliverable = "effect"
description = "CPU reference semantics pass."
verification = "deterministic"
state = "verified"

[[acceptance]]
id = "creative-approval"
deliverable = "effect"
description = "Owner approves the visual result."
verification = "human"
state = "review_needed"

[[external_truth]]
id = "owner-approval"
kind = "human_approval"
description = "Final creative approval is live owner state."
)toml";

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText, "spec.toml");
    const auto catalog = trace2d::agent::ParseCapabilityCatalogToml(CapabilityCatalog, "capabilities.toml");
    ASSERT_TRUE(spec.Succeeded());
    ASSERT_TRUE(catalog.Succeeded());

    const auto evaluation = trace2d::agent::EvaluateWork(*spec.spec, *catalog.catalog);
    EXPECT_EQ(evaluation.localReadiness, trace2d::agent::LocalReadiness::ReviewNeeded);
    ASSERT_EQ(evaluation.capabilityRequirements.size(), 1U);
    EXPECT_TRUE(evaluation.capabilityRequirements[0].eligible);
    EXPECT_EQ(evaluation.capabilityRequirements[0].reason, "eligible");
    ASSERT_EQ(evaluation.outstandingAcceptanceIds.size(), 1U);
    EXPECT_EQ(evaluation.outstandingAcceptanceIds[0], "creative-approval");
    ASSERT_EQ(evaluation.reviewAcceptanceIds.size(), 1U);
    EXPECT_EQ(evaluation.reviewAcceptanceIds[0], "creative-approval");
    EXPECT_TRUE(evaluation.RequiresLiveTruth());
}

TEST(WorkSpecTests, HumanVerifiedStillRequiresExplicitApproval)
{
    constexpr std::string_view specText = R"toml(
format_version = 1
[work]
id = "human-gate"
intent = "Keep human judgment non-automatic."
state = "verified"
constraints = []

[[deliverables]]
id = "result"
description = "Reviewable result"
state = "verified"

[[acceptance]]
id = "approval"
deliverable = "result"
description = "Human approves the result."
verification = "human"
state = "verified"
)toml";

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText);
    const trace2d::agent::CapabilityCatalog catalog{};
    ASSERT_TRUE(spec.Succeeded());

    const auto evaluation = trace2d::agent::EvaluateWork(*spec.spec, catalog);
    EXPECT_EQ(evaluation.localReadiness, trace2d::agent::LocalReadiness::ReviewNeeded);
    ASSERT_EQ(evaluation.outstandingAcceptanceIds.size(), 1U);
    EXPECT_EQ(evaluation.outstandingAcceptanceIds[0], "approval");
    ASSERT_EQ(evaluation.reviewAcceptanceIds.size(), 1U);
    EXPECT_EQ(evaluation.reviewAcceptanceIds[0], "approval");
}

TEST(WorkSpecTests, UnavailableCapabilityIsBlockedRatherThanImplementationFailure)
{
    constexpr std::string_view specText = R"toml(
format_version = 1
[work]
id = "physics-task"
intent = "Exercise Physics2D when it exists."
state = "planned"
constraints = []

[[deliverables]]
id = "physics"
description = "Physics interaction"
state = "planned"

[[requirements]]
deliverable = "physics"
capability = "physics2d"
minimum = "available"

[[acceptance]]
id = "contact"
deliverable = "physics"
description = "Contact is verified."
verification = "deterministic"
state = "planned"
)toml";

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText);
    const auto catalog = trace2d::agent::ParseCapabilityCatalogToml(CapabilityCatalog);
    ASSERT_TRUE(spec.Succeeded());
    ASSERT_TRUE(catalog.Succeeded());

    const auto evaluation = trace2d::agent::EvaluateWork(*spec.spec, *catalog.catalog);
    EXPECT_EQ(evaluation.localReadiness, trace2d::agent::LocalReadiness::Blocked);
    ASSERT_EQ(evaluation.capabilityRequirements.size(), 1U);
    EXPECT_FALSE(evaluation.capabilityRequirements[0].eligible);
    EXPECT_EQ(evaluation.capabilityRequirements[0].reason, "unavailable");
}

TEST(WorkSpecTests, ProductionMinimumDoesNotCollapseIntoTested)
{
    constexpr std::string_view specText = R"toml(
format_version = 1
[work]
id = "production-particle"
intent = "Require a production-supported particle capability."
state = "planned"
constraints = []

[[deliverables]]
id = "effect"
description = "Production effect"
state = "planned"

[[requirements]]
deliverable = "effect"
capability = "particles.cpu_reference"
minimum = "production_supported"

[[acceptance]]
id = "verify"
deliverable = "effect"
description = "Effect verifies."
verification = "deterministic"
state = "planned"
)toml";

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText);
    const auto catalog = trace2d::agent::ParseCapabilityCatalogToml(CapabilityCatalog);
    ASSERT_TRUE(spec.Succeeded());
    ASSERT_TRUE(catalog.Succeeded());

    const auto evaluation = trace2d::agent::EvaluateWork(*spec.spec, *catalog.catalog);
    EXPECT_EQ(evaluation.localReadiness, trace2d::agent::LocalReadiness::Blocked);
    ASSERT_EQ(evaluation.capabilityRequirements.size(), 1U);
    EXPECT_TRUE(evaluation.capabilityRequirements[0].available);
    EXPECT_TRUE(evaluation.capabilityRequirements[0].tested);
    EXPECT_FALSE(evaluation.capabilityRequirements[0].productionSupported);
    EXPECT_EQ(evaluation.capabilityRequirements[0].reason, "not_production_supported");
}

TEST(WorkSpecTests, CapabilityEvidenceIsRequiredForPositiveClaims)
{
    constexpr std::string_view catalogText = R"toml(
format_version = 1
[[capabilities]]
id = "unsupported-claim"
available = true
tested = true
production_supported = false
deterministic_verification = true
presentation_evidence = false
hardware_evidence = false
human_judgment = false
evidence = []
)toml";

    const auto catalog = trace2d::agent::ParseCapabilityCatalogToml(catalogText);
    EXPECT_FALSE(catalog.Succeeded());
    EXPECT_FALSE(catalog.diagnostics.empty());
}

TEST(WorkSpecTests, RejectsAcceptanceThatReferencesUnknownDeliverable)
{
    constexpr std::string_view specText = R"toml(
format_version = 1
[work]
id = "bad-ref"
intent = "Reject dangling semantic references."
state = "planned"
constraints = []

[[deliverables]]
id = "known"
description = "Known deliverable"
state = "planned"

[[acceptance]]
id = "dangling"
deliverable = "missing"
description = "Must fail parse."
verification = "deterministic"
state = "planned"
)toml";

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText);
    EXPECT_FALSE(spec.Succeeded());
    EXPECT_FALSE(spec.diagnostics.empty());
}
} // namespace
