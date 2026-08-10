#include <trace2d/agent/Workspace.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace
{
trace2d::agent::WorkSpec ParseSpec()
{
    constexpr std::string_view text = R"toml(
format_version = 1

[work]
id = "workspace-flow"
intent = "Review a repaired gameplay result and request a targeted revision."
state = "implemented"
constraints = []

[[deliverables]]
id = "combat"
description = "Representative combat slice"
state = "implemented"

[[acceptance]]
id = "semantic-proof"
deliverable = "combat"
description = "Authoritative combat behavior passes."
verification = "deterministic"
state = "implemented"

[[acceptance]]
id = "feel-review"
deliverable = "combat"
description = "A human reviews combat feel."
verification = "human"
state = "review_needed"

[[external_truth]]
id = "owner-approval"
kind = "human_approval"
description = "Final approval remains live human state."
)toml";
    auto parsed = trace2d::agent::ParseWorkSpecToml(text, "workspace-spec.toml");
    EXPECT_TRUE(parsed.Succeeded());
    return *parsed.spec;
}

trace2d::agent::WorkResult ParseRepairedResult()
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "workspace-flow"

[[revisions]]
id = "r1"
changed_paths = ["game/combat.cpp"]
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "failed"
summary = "Damage did not apply."
evidence = ["artifacts/r1/verify.json"]
failure_code = "damage_mismatch"
failure_target = "combat/damage"
failure_message = "Expected damage, observed none."
reproduction = "trace2d_verify combat"

[[revisions.feedback]]
id = "feedback-1"
target = "combat/damage"
message = "Repair damage before reviewing feel."

[[revisions]]
id = "r2"
parent = "r1"
changed_paths = ["game/combat.cpp", "content/effects/hit.trace2d.toml"]
limitations = ["Audio is not implemented yet."]

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "passed"
summary = "Damage now applies deterministically."
evidence = ["artifacts/r2/verify.json"]

[[revisions.verification]]
acceptance = "feel-review"
verification = "human"
outcome = "review_needed"
summary = "Combat is semantically correct and ready for feel review."
evidence = ["artifacts/r2/combat.webm"]

[[revisions.artifacts]]
id = "combat-preview"
kind = "video"
path = "artifacts/r2/combat.webm"
description = "Short gameplay review capture"
)toml";
    auto parsed = trace2d::agent::ParseWorkResultToml(text, "workspace-result.toml");
    EXPECT_TRUE(parsed.Succeeded());
    return *parsed.result;
}

TEST(WorkspaceTests, DerivesReviewSurfaceFromCurrentResultAndKeepsHistory)
{
    const auto snapshot = trace2d::agent::BuildWorkspaceSnapshot(ParseSpec(), ParseRepairedResult());

    EXPECT_EQ(snapshot.workId, "workspace-flow");
    EXPECT_EQ(snapshot.resultState, trace2d::agent::WorkResultState::ReviewNeeded);
    EXPECT_EQ(snapshot.currentRevisionId, "r2");
    ASSERT_EQ(snapshot.deliverables.size(), 1U);
    EXPECT_EQ(snapshot.deliverables[0].state, trace2d::agent::WorkspaceProgressState::ReviewNeeded);
    ASSERT_EQ(snapshot.reviewQueue.size(), 1U);
    EXPECT_EQ(snapshot.reviewQueue[0].acceptanceId, "feel-review");
    EXPECT_EQ(snapshot.reviewQueue[0].target, "acceptance/feel-review");
    ASSERT_EQ(snapshot.currentArtifacts.size(), 1U);
    EXPECT_EQ(snapshot.currentArtifacts[0].path, "artifacts/r2/combat.webm");
    ASSERT_EQ(snapshot.currentLimitations.size(), 1U);
    ASSERT_EQ(snapshot.revisions.size(), 2U);
    EXPECT_EQ(snapshot.revisions[0].failedVerificationCount, 1U);
    ASSERT_EQ(snapshot.revisions[0].feedback.size(), 1U);
    EXPECT_EQ(snapshot.revisions[0].feedback[0].target, "combat/damage");
    EXPECT_TRUE(snapshot.externalTruth.size() == 1U);
}

TEST(WorkspaceTests, MachineOwnedReviewNeededNeverBecomesHumanReviewQueue)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[result]
work_id = "workspace-flow"

[[revisions]]
id = "r1"
changed_paths = []
limitations = []

[[revisions.verification]]
acceptance = "semantic-proof"
verification = "deterministic"
outcome = "review_needed"
summary = "This machine-owned check still needs real verification."
evidence = []

[[revisions.verification]]
acceptance = "feel-review"
verification = "human"
outcome = "review_needed"
summary = "Human review is requested too early."
evidence = ["artifacts/r1/capture.png"]
)toml";
    const auto parsed = trace2d::agent::ParseWorkResultToml(text);
    ASSERT_TRUE(parsed.Succeeded());

    const auto snapshot = trace2d::agent::BuildWorkspaceSnapshot(ParseSpec(), *parsed.result);
    EXPECT_EQ(snapshot.resultState, trace2d::agent::WorkResultState::Incomplete);
    EXPECT_TRUE(snapshot.reviewQueue.empty());
    ASSERT_EQ(snapshot.deliverables.size(), 1U);
    EXPECT_EQ(snapshot.deliverables[0].state, trace2d::agent::WorkspaceProgressState::Working);
}

TEST(WorkspaceTests, FeedbackPacketRoundTripsAndTargetsCurrentRevision)
{
    const auto snapshot = trace2d::agent::BuildWorkspaceSnapshot(ParseSpec(), ParseRepairedResult());
    trace2d::agent::WorkspaceAction action{};
    action.kind = trace2d::agent::WorkspaceActionKind::Feedback;
    action.workId = snapshot.workId;
    action.revisionId = snapshot.currentRevisionId;
    action.acceptanceId = "feel-review";
    action.target = "combat/hit-effect";
    action.message = "Make the hit feel heavier without changing damage.";

    EXPECT_TRUE(trace2d::agent::ValidateWorkspaceAction(snapshot, action).empty());
    const std::string text = trace2d::agent::SerializeWorkspaceActionToml(action);
    const auto parsed = trace2d::agent::ParseWorkspaceActionToml(text, "workspace-action.toml");
    ASSERT_TRUE(parsed.Succeeded());
    EXPECT_EQ(parsed.action->kind, trace2d::agent::WorkspaceActionKind::Feedback);
    EXPECT_EQ(parsed.action->revisionId, "r2");
    EXPECT_EQ(parsed.action->acceptanceId, "feel-review");
    EXPECT_EQ(parsed.action->target, "combat/hit-effect");
    EXPECT_EQ(parsed.action->message, "Make the hit feel heavier without changing damage.");
}

TEST(WorkspaceTests, ApprovalIsRestrictedToCurrentReviewQueueAndRevision)
{
    const auto snapshot = trace2d::agent::BuildWorkspaceSnapshot(ParseSpec(), ParseRepairedResult());

    trace2d::agent::WorkspaceAction approval{};
    approval.kind = trace2d::agent::WorkspaceActionKind::Approve;
    approval.workId = snapshot.workId;
    approval.revisionId = snapshot.currentRevisionId;
    approval.acceptanceId = "feel-review";
    approval.target = "acceptance/feel-review";
    EXPECT_TRUE(trace2d::agent::ValidateWorkspaceAction(snapshot, approval).empty());

    approval.revisionId = "r1";
    EXPECT_FALSE(trace2d::agent::ValidateWorkspaceAction(snapshot, approval).empty());

    approval.revisionId = snapshot.currentRevisionId;
    approval.acceptanceId = "semantic-proof";
    approval.target = "acceptance/semantic-proof";
    EXPECT_FALSE(trace2d::agent::ValidateWorkspaceAction(snapshot, approval).empty());
}

TEST(WorkspaceTests, RejectsUnknownActionFields)
{
    constexpr std::string_view text = R"toml(
format_version = 1
[action]
kind = "feedback"
work_id = "workspace-flow"
revision = "r2"
message = "Revise it."
unexpected = true
)toml";
    const auto parsed = trace2d::agent::ParseWorkspaceActionToml(text);
    EXPECT_FALSE(parsed.Succeeded());
    EXPECT_FALSE(parsed.diagnostics.empty());
}
} // namespace
