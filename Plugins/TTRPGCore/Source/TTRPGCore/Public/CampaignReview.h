#pragma once

#include "CoreMinimal.h"

namespace TTRPG
{
// Plain domain records deliberately have no actor, widget, or network dependency.
struct TTRPGCORE_API FCampaignRecord
{
    FGuid OwnerId;
    TSet<FGuid> Editors;
    int64 Revision = 0;
    TMap<FString, FString> Properties;
};

struct TTRPGCORE_API FPropertyEdit
{
    FGuid RecordId;
    int64 BaseRevision = 0;
    FString Property;
    FString Value;
};

struct TTRPGCORE_API FChangeSubmission
{
    FGuid CampaignId;
    FGuid SubmissionId;
    TArray<FPropertyEdit> Edits;
};

struct TTRPGCORE_API FReviewPolicy
{
    bool bAutoAcceptMinorEdits = false;
    TSet<FString> MinorProperties;
};

enum class EHostDecision : uint8
{
    EvaluatePolicy,
    Approve,
    Reject
};

enum class EReviewStatus : uint8
{
    Invalid,
    Unauthorized,
    Conflict,
    NeedsReview,
    Accepted,
    Rejected,
    AlreadyDecided
};

struct TTRPGCORE_API FReviewedEdit
{
    FPropertyEdit Edit;
    FString PreviousValue;
};

struct TTRPGCORE_API FDecisionRecord
{
    FGuid AuthorId;
    EReviewStatus Status = EReviewStatus::Invalid;
    FDateTime DecidedAt;
    bool bAutomatic = false;
    TArray<FReviewedEdit> Edits;
};

struct TTRPGCORE_API FCampaignState
{
    FGuid CampaignId;
    TMap<FGuid, FCampaignRecord> Records;
    TMap<FGuid, FDecisionRecord> Decisions;
};

struct TTRPGCORE_API FReviewResult
{
    EReviewStatus Status;
    FString Reason;
};

// Call on the authoritative host's serialized command queue. AuthorId must come
// from authenticated transport; Approve/Reject must come from an authorized DM.
// This is an in-memory operation. Persist state + ledger before acknowledging.
TTRPGCORE_API FReviewResult ReviewSubmission(
    FCampaignState& Campaign,
    const FGuid& AuthorId,
    const FChangeSubmission& Submission,
    const FReviewPolicy& Policy,
    EHostDecision Decision = EHostDecision::EvaluatePolicy);
}
