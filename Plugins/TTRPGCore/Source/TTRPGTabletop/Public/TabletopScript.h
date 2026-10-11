#pragma once
#include "CoreMinimal.h"

namespace TTRPG::Tabletop
{
// Saved source only; loading a campaign never executes scripts.
struct TTRPGTABLETOP_API FTabletopScript
{
    int32 Id = 0;
    FString Name;
    FString Source;
    int32 OwnerId = 0; // 0: DM authored. Player-authored scripts are manual only.
    bool bPlayerCallable = false;
    int32 MapId = 1;
    int32 TokenId = 0; // 0: map binding; otherwise a token instance.
    FString Event = TEXT("manual");
    bool bEnabled = true;
    FString Condition; // Optional required condition on the event token.
};

struct TTRPGTABLETOP_API FScriptToken
{
    int32 Id = 0;
    int32 MapId = 0;
    int32 OwnerId = 0;
    FString Name;
    TMap<FString, double> Attributes;
    TArray<FString> Conditions;
    TSet<FString> WritableAttributes;
};

// The host constructs this snapshot using the caller's current authority. Do not
// put hidden tokens or private attributes in a player's snapshot.
struct TTRPGTABLETOP_API FScriptContext
{
    bool bDM = false;
    int32 PlayerId = 0;
    int32 SelfId = 0;
    int32 MapId = 0;
    FString Event = TEXT("manual");
    FString EventCondition;
    FString MapName;
    TMap<FString, double> MapAttributes;
    TArray<FScriptToken> Tokens;
    int32 RandomSeed = 12345;
};

enum class EScriptCommand : uint8 { SetAttribute, AddCondition, RemoveCondition };
struct TTRPGTABLETOP_API FScriptCommand
{
    EScriptCommand Kind = EScriptCommand::SetAttribute;
    int32 TargetId = 0;
    FString Name;
    double Value = 0;
};
struct TTRPGTABLETOP_API FScriptResult
{
    bool bSuccess = false;
    FString Error;
    TArray<FString> Logs;
    TArray<FScriptCommand> Commands;
};

class TTRPGTABLETOP_API FTabletopScriptRuntime
{
public:
    // Fresh bounded Lua 5.4 state per run. No world objects, native handles, files,
    // processes, network, packages, dynamic loading, debug, or nested execution.
    // Success returns proposals: the caller must revalidate/apply them atomically.
    static FScriptResult Run(const FString& Source, const FScriptContext& Context);
    static constexpr int32 MaxSourceLength = 16384;
    static constexpr int32 MaxCommands = 64;
    static constexpr int32 MaxLogs = 32;
    static constexpr int32 MaxInstructions = 100000;
    static constexpr SIZE_T MaxMemory = 2 * 1024 * 1024;
};
}
