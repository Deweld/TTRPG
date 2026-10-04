#include "CampaignReview.h"

namespace TTRPG
{
FReviewResult ReviewSubmission(
    FCampaignState& Campaign,
    const FGuid& AuthorId,
    const FChangeSubmission& Submission,
    const FReviewPolicy& Policy,
    EHostDecision Decision)
{
    if (!AuthorId.IsValid() || !Campaign.CampaignId.IsValid()
        || Submission.CampaignId != Campaign.CampaignId || !Submission.SubmissionId.IsValid()
        || Submission.Edits.IsEmpty() || Submission.Edits.Num() > 256)
    {
        return { EReviewStatus::Invalid, TEXT("Invalid submission identity, campaign, or edit count.") };
    }

    if (const FDecisionRecord* Existing = Campaign.Decisions.Find(Submission.SubmissionId))
    {
        if (Existing->AuthorId != AuthorId || Existing->Edits.Num() != Submission.Edits.Num())
        {
            return { EReviewStatus::Invalid, TEXT("Submission ID has already been used for another proposal.") };
        }
        for (int32 Index = 0; Index < Submission.Edits.Num(); ++Index)
        {
            const FPropertyEdit& Before = Existing->Edits[Index].Edit;
            const FPropertyEdit& After = Submission.Edits[Index];
            if (Before.RecordId != After.RecordId || Before.BaseRevision != After.BaseRevision
                || Before.Property != After.Property || Before.Value != After.Value)
            {
                return { EReviewStatus::Invalid, TEXT("Submission ID has already been used for another proposal.") };
            }
        }
        return { EReviewStatus::AlreadyDecided, TEXT("This submission already has a recorded decision.") };
    }

    // Validate the entire proposal before mutating any record or decision history.
    TMap<FGuid, TSet<FString>> TouchedProperties;
    FDecisionRecord Audit;
    Audit.AuthorId = AuthorId;
    bool bAllMinor = true;
    for (const FPropertyEdit& Edit : Submission.Edits)
    {
        const FCampaignRecord* Record = Campaign.Records.Find(Edit.RecordId);
        if (!Record || (Record->OwnerId != AuthorId && !Record->Editors.Contains(AuthorId)))
        {
            return { EReviewStatus::Unauthorized, TEXT("A requested record is unavailable or not editable.") };
        }
        if (Edit.BaseRevision < 0 || Record->Revision != Edit.BaseRevision)
        {
            return { EReviewStatus::Conflict, TEXT("A record changed after the player's approved snapshot.") };
        }
        const FString* PreviousValue = Record->Properties.Find(Edit.Property);
        if (!PreviousValue || Edit.Property.IsEmpty() || Edit.Property.Len() > 128
            || Edit.Value.Len() > 16384 || Record->Revision == MAX_int64)
        {
            return { EReviewStatus::Invalid, TEXT("Invalid property, value size, or exhausted revision.") };
        }
        TSet<FString>& Seen = TouchedProperties.FindOrAdd(Edit.RecordId);
        if (Seen.Contains(Edit.Property))
        {
            return { EReviewStatus::Invalid, TEXT("A proposal cannot edit the same property twice.") };
        }
        Seen.Add(Edit.Property);
        bAllMinor &= Policy.MinorProperties.Contains(Edit.Property);
        Audit.Edits.Add({ Edit, *PreviousValue });
    }

    if (Decision == EHostDecision::EvaluatePolicy && !(Policy.bAutoAcceptMinorEdits && bAllMinor))
    {
        return { EReviewStatus::NeedsReview, TEXT("DM review is required by the current policy.") };
    }
    if (Decision != EHostDecision::EvaluatePolicy && Decision != EHostDecision::Approve
        && Decision != EHostDecision::Reject)
    {
        return { EReviewStatus::Invalid, TEXT("Unknown host decision.") };
    }

    Audit.Status = Decision == EHostDecision::Reject ? EReviewStatus::Rejected : EReviewStatus::Accepted;
    Audit.DecidedAt = FDateTime::UtcNow();
    Audit.bAutomatic = Decision == EHostDecision::EvaluatePolicy;
    if (Audit.Status == EReviewStatus::Accepted)
    {
        for (const FPropertyEdit& Edit : Submission.Edits)
        {
            Campaign.Records.FindChecked(Edit.RecordId).Properties.FindChecked(Edit.Property) = Edit.Value;
        }
        for (const auto& Pair : TouchedProperties)
        {
            ++Campaign.Records.FindChecked(Pair.Key).Revision;
        }
    }
    const EReviewStatus Status = Audit.Status;
    Campaign.Decisions.Add(Submission.SubmissionId, MoveTemp(Audit));
    return { Status, Status == EReviewStatus::Accepted ? TEXT("Changes accepted.") : TEXT("Changes rejected.") };
}
}
