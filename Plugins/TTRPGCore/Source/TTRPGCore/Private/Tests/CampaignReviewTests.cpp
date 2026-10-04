#include "CampaignReview.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
using namespace TTRPG;

struct FFixture
{
    FGuid Player = FGuid::NewGuid();
    FGuid RecordId = FGuid::NewGuid();
    FCampaignState Campaign;
    FReviewPolicy Policy;
    FChangeSubmission Submission;

    FFixture()
    {
        Campaign.CampaignId = FGuid::NewGuid();
        FCampaignRecord Record;
        Record.OwnerId = Player;
        Record.Properties.Add(TEXT("notes"), TEXT("Original"));
        Record.Properties.Add(TEXT("hp"), TEXT("10"));
        Campaign.Records.Add(RecordId, MoveTemp(Record));
        Submission.CampaignId = Campaign.CampaignId;
        Submission.SubmissionId = FGuid::NewGuid();
        Submission.Edits.Add({ RecordId, 0, TEXT("notes"), TEXT("Updated offline") });
        Policy.MinorProperties.Add(TEXT("notes"));
    }

    FReviewResult Review(EHostDecision Decision = EHostDecision::EvaluatePolicy)
    {
        return ReviewSubmission(Campaign, Player, Submission, Policy, Decision);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualReviewTest, "TTRPG.Review.ManualByDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FManualReviewTest::RunTest(const FString& Parameters)
{
    FFixture F;
    TestTrue(TEXT("Minor edits still require review by default"), F.Review().Status == EReviewStatus::NeedsReview);
    TestEqual(TEXT("Evaluation does not change state"), F.Campaign.Records[F.RecordId].Revision, int64(0));
    TestEqual(TEXT("Evaluation is not a final decision"), F.Campaign.Decisions.Num(), 0);
    TestTrue(TEXT("DM approval applies changes"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Accepted);
    TestEqual(TEXT("New value"), F.Campaign.Records[F.RecordId].Properties[TEXT("notes")], FString(TEXT("Updated offline")));
    const FDecisionRecord& Audit = F.Campaign.Decisions[F.Submission.SubmissionId];
    TestEqual(TEXT("Audit preserves previous value"), Audit.Edits[0].PreviousValue, FString(TEXT("Original")));
    TestTrue(TEXT("Audit records authenticated author"), Audit.AuthorId == F.Player);
    TestFalse(TEXT("Explicit approval is not automatic"), Audit.bAutomatic);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMinorPolicyTest, "TTRPG.Review.MinorPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMinorPolicyTest::RunTest(const FString& Parameters)
{
    FFixture Minor;
    Minor.Policy.bAutoAcceptMinorEdits = true;
    TestTrue(TEXT("Allowlisted minor edit accepted"), Minor.Review().Status == EReviewStatus::Accepted);
    TestTrue(TEXT("Automatic decision recorded"), Minor.Campaign.Decisions[Minor.Submission.SubmissionId].bAutomatic);
    FFixture Major;
    Major.Policy.bAutoAcceptMinorEdits = true;
    Major.Submission.Edits.Add({ Major.RecordId, 0, TEXT("hp"), TEXT("100") });
    TestTrue(TEXT("One major edit holds the whole proposal"), Major.Review().Status == EReviewStatus::NeedsReview);
    TestEqual(TEXT("Minor part was not partially applied"), Major.Campaign.Records[Major.RecordId].Properties[TEXT("notes")], FString(TEXT("Original")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPermissionTest, "TTRPG.Review.CurrentPermissions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPermissionTest::RunTest(const FString& Parameters)
{
    FFixture F;
    F.Campaign.Records[F.RecordId].OwnerId = FGuid::NewGuid();
    TestTrue(TEXT("Even approval cannot bypass edit permission"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Unauthorized);
    F.Campaign.Records[F.RecordId].Editors.Add(F.Player);
    TestTrue(TEXT("Explicit grant allows submission for review"), F.Review().Status == EReviewStatus::NeedsReview);
    F.Campaign.Records[F.RecordId].Editors.Remove(F.Player);
    TestTrue(TEXT("Revoked grant is checked again at approval"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Unauthorized);
    TestEqual(TEXT("No unauthorized decisions"), F.Campaign.Decisions.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConflictTest, "TTRPG.Review.StaleRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConflictTest::RunTest(const FString& Parameters)
{
    FFixture F;
    TestTrue(TEXT("Initially queued for review"), F.Review().Status == EReviewStatus::NeedsReview);
    F.Campaign.Records[F.RecordId].Revision = 1;
    F.Policy.bAutoAcceptMinorEdits = true;
    TestTrue(TEXT("Stale minor change cannot auto-apply"), F.Review().Status == EReviewStatus::Conflict);
    TestTrue(TEXT("DM approval also revalidates revisions"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Conflict);
    TestEqual(TEXT("Conflict preserves current value"), F.Campaign.Records[F.RecordId].Properties[TEXT("notes")], FString(TEXT("Original")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAtomicityTest, "TTRPG.Review.AtomicProposal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAtomicityTest::RunTest(const FString& Parameters)
{
    FFixture F;
    F.Submission.Edits.Add({ FGuid::NewGuid(), 0, TEXT("notes"), TEXT("Forbidden") });
    TestTrue(TEXT("Unavailable second record rejects entire proposal"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Unauthorized);
    TestEqual(TEXT("First record was not modified"), F.Campaign.Records[F.RecordId].Properties[TEXT("notes")], FString(TEXT("Original")));
    TestEqual(TEXT("Revision was not incremented"), F.Campaign.Records[F.RecordId].Revision, int64(0));
    TestEqual(TEXT("No decision was stored"), F.Campaign.Decisions.Num(), 0);
    F.Submission.Edits.Pop();
    F.Submission.Edits.Add({ F.RecordId, 0, TEXT("hp"), TEXT("9") });
    TestTrue(TEXT("Valid multi-field proposal accepted"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Accepted);
    TestEqual(TEXT("One revision per record, not per field"), F.Campaign.Records[F.RecordId].Revision, int64(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplayTest, "TTRPG.Review.ReplayProtection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FReplayTest::RunTest(const FString& Parameters)
{
    FFixture F;
    TestTrue(TEXT("First approval accepted"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Accepted);
    TestTrue(TEXT("Retry recognized despite old revision"), F.Review(EHostDecision::Approve).Status == EReviewStatus::AlreadyDecided);
    TestEqual(TEXT("Retry does not increment revision"), F.Campaign.Records[F.RecordId].Revision, int64(1));
    TestEqual(TEXT("Retry does not duplicate audit"), F.Campaign.Decisions.Num(), 1);
    F.Submission.Edits[0].Value = TEXT("Different payload");
    TestTrue(TEXT("Cannot reuse decided ID with another payload"), F.Review(EHostDecision::Approve).Status == EReviewStatus::Invalid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRejectionTest, "TTRPG.Review.Rejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRejectionTest::RunTest(const FString& Parameters)
{
    FFixture F;
    TestTrue(TEXT("DM rejection recorded"), F.Review(EHostDecision::Reject).Status == EReviewStatus::Rejected);
    TestEqual(TEXT("Rejected value unchanged"), F.Campaign.Records[F.RecordId].Properties[TEXT("notes")], FString(TEXT("Original")));
    TestEqual(TEXT("Rejected revision unchanged"), F.Campaign.Records[F.RecordId].Revision, int64(0));
    TestTrue(TEXT("Rejected proposal cannot be replayed as an approval"), F.Review(EHostDecision::Approve).Status == EReviewStatus::AlreadyDecided);
    TestEqual(TEXT("Audit still shows rejection"), int32(F.Campaign.Decisions[F.Submission.SubmissionId].Status), int32(EReviewStatus::Rejected));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInvalidProposalTest, "TTRPG.Review.InvalidProposals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInvalidProposalTest::RunTest(const FString& Parameters)
{
    FFixture WrongCampaign;
    WrongCampaign.Submission.CampaignId = FGuid::NewGuid();
    TestTrue(TEXT("Cross-campaign proposal rejected"), WrongCampaign.Review(EHostDecision::Approve).Status == EReviewStatus::Invalid);
    FFixture Empty;
    Empty.Submission.Edits.Empty();
    TestTrue(TEXT("Empty proposal rejected"), Empty.Review(EHostDecision::Approve).Status == EReviewStatus::Invalid);
    FFixture Unknown;
    Unknown.Submission.Edits[0].Property = TEXT("owner");
    TestTrue(TEXT("Unknown property cannot grant ownership"), Unknown.Review(EHostDecision::Approve).Status == EReviewStatus::Invalid);
    FFixture Duplicate;
    const FPropertyEdit Copy = Duplicate.Submission.Edits[0];
    Duplicate.Submission.Edits.Add(Copy);
    TestTrue(TEXT("Duplicate property rejected"), Duplicate.Review(EHostDecision::Approve).Status == EReviewStatus::Invalid);
    FFixture Unauthenticated;
    Unauthenticated.Player.Invalidate();
    TestTrue(TEXT("Invalid caller identity rejected"), Unauthenticated.Review(EHostDecision::Approve).Status == EReviewStatus::Invalid);
    return true;
}
#endif
