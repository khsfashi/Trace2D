#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace
{
trace2d::agent::WorkSpec ParseRepresentativeSpec()
{
    constexpr std::string_view text = R"toml(
format_version = 1

[work]
id = "repair-flow"
intent = "Repair a semantic failure and preserve the review boundary."
state = "implemented"
constraints = []

[[deliverables]]
id = "behavior"
description = "Verified behavior"
state = "implemented"

[[acceptance]]
id = "semantic-proof"
deliverable = "behavior"
description = "Deterministic behavior passes."
verification = "deterministic"
state = "implemented"

[[acceptance]]
id = "owner-review"
deliverable = "behavior"
description = "Owner reviews the presentation."
verification = "human"
state = "review_needed"

[[external_truth]]
id = "owner-approval"
kind = "human_approval"
description = "Final approval remains live human state."
)toml";

    auto parsed = trace2d::agent::ParseWorkSpecToml(text, "work.toml");
    EXPECT_TRUE(parsed.Succeeded());
    return *parsed.spec;
}

TEST(WorkResultTests, PreservesFailedRevisionAndEvaluatesRepairedCurrentRevision)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "repair-flow"

[[revisions]]
id = "r1"
changed_paths = ["game/player.cpp"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "failed"
summary = "Player state did not change after input."
evidence = ["artifacts/r1/verify.json"]
failure_code = "state_mismatch"
failure_target = "scene/player.state"
failure_message = "Expected moving, observed idle."
reproduction = "trace2d verify --scene tests/data/player.scene.toml"

[[revisions.verification]]
acceptance = "owner-review"
verification = "human"
outcome = "not_run"
summary = "Presentation review waits for semantic repair."
evidence = []

[[revisions]]
id = "r2"
parent = "r1"
changed_paths = ["game/player.cpp"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "passed"
summary = "Input now reaches authoritative player state."
evidence = ["artifacts/r2/verify.json"]

[[revisions.verification]]
acceptance = "owner-review"
verification = "human"
outcome = "review_needed"
summary = "Deterministic checks pass; presentation awaits owner review."
evidence = ["artifacts/r2/capture.png"]

[[revisions.artifacts]]
id = "preview"
kind = "capture"
path = "artifacts/r2/capture.png"
description = "Exact-frame presentation evidence"
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text, "result.toml");
    ASSERT_TRUE(parsed.Succeeded());
    ASSERT_EQ(parsed.result->revisions.size(), 2U);
    ASSERT_EQ(parsed.result->revisions[0].verification.size(), 2U);
    ASSERT_TRUE(parsed.result->revisions[0].verification[0].failure.has_value());
    EXPECT_EQ(parsed.result->revisions[0].verification[0].failure->code, "state_mismatch");
    EXPECT_EQ(parsed.result->revisions[1].parentRevisionId, "r1");

    const auto evaluation = trace2d::agent::EvaluateWorkResult(ParseRepresentativeSpec(), *parsed.result);
    EXPECT_EQ(evaluation.state, trace2d::agent::WorkResultState::ReviewNeeded);
    EXPECT_EQ(evaluation.currentRevisionId, "r2");
    EXPECT_TRUE(evaluation.failures.empty());
    EXPECT_TRUE(evaluation.outstandingAcceptanceIds.empty());
    ASSERT_EQ(evaluation.reviewAcceptanceIds.size(), 1U);
    EXPECT_EQ(evaluation.reviewAcceptanceIds[0], "owner-review");
    EXPECT_TRUE(evaluation.RequiresLiveTruth());
}

TEST(WorkResultTests, HumanApprovalCompletesOnlyAfterApprovedOutcome)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "repair-flow"

[[revisions]]
id = "r1"
changed_paths = ["game/player.cpp"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "passed"
summary = "Semantic verification passed."
evidence = ["artifacts/r1/verify.json"]

[[revisions.verification]]
acceptance = "owner-review"
verification = "human"
outcome = "approved"
summary = "Owner approved the result."
evidence = ["artifacts/r1/owner-approval.txt"]
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text);
    ASSERT_TRUE(parsed.Succeeded());
    const auto evaluation = trace2d::agent::EvaluateWorkResult(ParseRepresentativeSpec(), *parsed.result);
    EXPECT_EQ(evaluation.state, trace2d::agent::WorkResultState::Complete);
    EXPECT_TRUE(evaluation.outstandingAcceptanceIds.empty());
    EXPECT_TRUE(evaluation.reviewAcceptanceIds.empty());
    EXPECT_TRUE(evaluation.RequiresLiveTruth());
}

TEST(WorkResultTests, DeterministicFailureReturnsStructuredReproductionContext)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "repair-flow"

[[revisions]]
id = "r1"
changed_paths = ["game/player.cpp"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "failed"
summary = "Authoritative state mismatched."
evidence = ["artifacts/r1/verify.json"]
failure_code = "state_mismatch"
failure_target = "scene/player.state"
failure_message = "Expected moving, observed idle."
reproduction = "trace2d verify --scene tests/data/player.scene.toml"

[[revisions.verification]]
acceptance = "owner-review"
verification = "human"
outcome = "not_run"
summary = "Review has not started."
evidence = []
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text);
    ASSERT_TRUE(parsed.Succeeded());
    const auto evaluation = trace2d::agent::EvaluateWorkResult(ParseRepresentativeSpec(), *parsed.result);
    EXPECT_EQ(evaluation.state, trace2d::agent::WorkResultState::Failed);
    ASSERT_EQ(evaluation.failures.size(), 1U);
    EXPECT_EQ(evaluation.failures[0].code, "state_mismatch");
    EXPECT_EQ(evaluation.failures[0].target, "scene/player.state");
    EXPECT_FALSE(evaluation.failures[0].reproduction.empty());
}

TEST(WorkResultTests, PassedVerificationRequiresEvidence)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "repair-flow"

[[revisions]]
id = "r1"
changed_paths = []
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "passed"
summary = "Claimed success without evidence."
evidence = []
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_FALSE(parsed.diagnostics.empty());
}

TEST(WorkResultTests, RevisionLineageMustBeLinearAndExplicit)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "repair-flow"

[[revisions]]
id = "r1"
changed_paths = []
limitations = []

[[revisions]]
id = "r2"
parent = "wrong-parent"
changed_paths = []
limitations = []
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_FALSE(parsed.diagnostics.empty());
}

TEST(WorkResultTests, MalformedPrecedingRevisionIsDiagnosedWithoutUnsafeLineageAccess)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "repair-flow"

revisions = [
    7,
    { id = "r2", parent = "r1", changed_paths = [], limitations = [] }
]
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text, "malformed.toml");
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_GE(parsed.diagnostics.size(), 2U);
}

TEST(WorkResultTests, UnknownFieldsAndStaleFailureFieldsAreRejected)
{
    constexpr std::string_view text = R"toml(
format_version = 1
unexpected_root = true
[result]
work_id = "repair-flow"

[[revisions]]
id = "r1"
changed_paths = []
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "passed"
summary = "Passed but carries stale failure metadata."
evidence = ["artifacts/r1/verify.json"]
failure_code = "stale"
)toml";

    const auto parsed = trace2d::agent::ParseWorkResultToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_GE(parsed.diagnostics.size(), 2U);
}
} // namespace
