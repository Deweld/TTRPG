#include "TabletopModel.h"
#include "TabletopScript.h"

namespace TTRPG::Tabletop
{
namespace
{
bool ValidEvent(const FString& E)
{
    return E==TEXT("manual") || E==TEXT("move") || E==TEXT("turn_start") || E==TEXT("condition_added");
}
bool PlayerWritable(const FToken& T,const FString& Key)
{
    if(Key.Equals(TEXT("hp"),ESearchCase::IgnoreCase)) return true;
    if(Key.Equals(TEXT("maxHP"),ESearchCase::IgnoreCase) || Key.Equals(TEXT("defense"),ESearchCase::IgnoreCase) || Key.Equals(TEXT("speed"),ESearchCase::IgnoreCase) || !T.Attributes.Contains(Key)) return false;
    return T.Bars.ContainsByPredicate([&](const FStatusBar& B){return B.ValueAttribute.Equals(Key,ESearchCase::IgnoreCase);})
        && !T.Bars.ContainsByPredicate([&](const FStatusBar& B){return B.MaxAttribute.Equals(Key,ESearchCase::IgnoreCase);});
}
}
bool FBoardModel::CanReadScript(const FTabletopScript& S) const
{
    if(S.MapId!=ActiveMapId) return false;
    if(IsDM()) return true;
    if(S.TokenId) {const auto* T=Find(S.TokenId); if(!T || !IsVisible(*T)) return false;}
    return S.Event==TEXT("manual") && (S.OwnerId==ViewingPlayerId || (S.OwnerId==0 && S.bPlayerCallable));
}
int32 FBoardModel::SaveScript(const FTabletopScript& Input)
{
    if(Workspace!=EWorkspace::Script) {Status=TEXT("Open Script mode to edit macros and triggers.");return INDEX_NONE;}
    const FTabletopScript* Old=Scripts.FindByPredicate([&](const FTabletopScript& S){return S.Id==Input.Id;});
    if(Input.Id && !Old) return INDEX_NONE;
    if(Old && (!CanReadScript(*Old) || (!IsDM() && Old->OwnerId!=ViewingPlayerId))) return INDEX_NONE;
    FTabletopScript S=Input;
    if(!IsDM()) {S.OwnerId=ViewingPlayerId;S.bPlayerCallable=false;S.Event=TEXT("manual");}
    else if(!Old) S.OwnerId=0;
    else S.OwnerId=Old->OwnerId;
    const FToken* Bound=S.TokenId?Find(S.TokenId):nullptr;
    if(S.MapId!=ActiveMapId || !FindMap(S.MapId) || S.Name.TrimStartAndEnd().IsEmpty() || S.Name.Len()>80
        || S.Source.Len()>16384 || FTCHARToUTF8(*S.Source).Length()>16384 || !ValidEvent(S.Event) || S.Condition.Len()>64
        || (S.TokenId && (!Bound || Bound->MapId!=S.MapId || !IsVisible(*Bound) || (!IsDM() && !CanControl(*Bound))))
        || (S.OwnerId && Bound && Bound->OwnerId!=S.OwnerId)
        || (S.OwnerId && S.Event!=TEXT("manual")) || (!Old && Scripts.Num()>=64))
    {Status=TEXT("Script settings are invalid for this role or map.");return INDEX_NONE;}
    if(!Old)
    {
        // Reuse a free bounded identifier; a valid imported high ID must not
        // force the next save outside the persistence format's ID range.
        S.Id=1;
        while(Scripts.ContainsByPredicate([&](const FTabletopScript& Existing){return Existing.Id==S.Id;})) ++S.Id;
        if(S.Id>1000000) return INDEX_NONE;
        Scripts.Add(S);
    }
    else *Scripts.FindByPredicate([&](const FTabletopScript& V){return V.Id==S.Id;})=S;
    Log(TEXT("Script saved. Automatic triggers run during Game Play only."));return S.Id;
}
bool FBoardModel::DeleteScript(int32 Id)
{
    const auto* S=Scripts.FindByPredicate([&](const FTabletopScript& V){return V.Id==Id;});
    if(Workspace!=EWorkspace::Script || !S || !CanReadScript(*S) || (!IsDM() && S->OwnerId!=ViewingPlayerId)) return false;
    Scripts.RemoveAll([=](const FTabletopScript& V){return V.Id==Id;});Log(TEXT("Script removed."));return true;
}
bool FBoardModel::ExecuteScript(const FTabletopScript& S,int32 SelfId,const FString& Event,const FString& EventCondition)
{
    FScriptContext Context;Context.bDM=IsDM();Context.PlayerId=ViewingPlayerId;Context.MapId=ActiveMapId;
    Context.SelfId=S.TokenId?S.TokenId:FMath::Max(0,SelfId);Context.Event=Event;Context.MapName=Map().Name;Context.RandomSeed=FMath::Rand();
    Context.MapAttributes={{TEXT("width"),double(Map().Width)},{TEXT("height"),double(Map().Height)},
        {TEXT("scale"),Map().UnitsPerCell},{TEXT("round"),double(Map().Round)},{TEXT("combat"),Map().bCombat?1.:0.}};
    const FToken* Self=Find(Context.SelfId);
    if(Context.SelfId>0 && (!Self || !IsVisible(*Self))) {Status=TEXT("Script token is unavailable.");return false;}
    if(!S.Condition.IsEmpty() && (!Self || !Self->Conditions.ContainsByPredicate([&](const FString& C){return C.Equals(S.Condition,ESearchCase::IgnoreCase);}))) {Status=TEXT("Script condition is not met.");return false;}
    Context.EventCondition=EventCondition;
    for(const auto& T:Tokens)
    {
        if(!IsVisible(T)) continue;
        FScriptToken V;V.Id=T.Id;V.MapId=T.MapId;V.OwnerId=T.OwnerId;V.Name=T.Name;V.Conditions=T.Conditions;
        for(const auto& A:T.Attributes)V.Attributes.Add(A.Key,A.Value);
        V.Attributes.Add(TEXT("hp"),T.HP);V.Attributes.Add(TEXT("maxHP"),T.Sheet.MaxHP);
        V.Attributes.Add(TEXT("defense"),T.Sheet.Defense);V.Attributes.Add(TEXT("speed"),T.Sheet.Speed);
        V.Attributes.Add(TEXT("x"),T.Cell.X);V.Attributes.Add(TEXT("y"),T.Cell.Y);
        if(T.OwnerId==ViewingPlayerId)for(const auto& A:V.Attributes)if(PlayerWritable(T,A.Key))V.WritableAttributes.Add(A.Key);
        Context.Tokens.Add(MoveTemp(V));
    }
    const FScriptResult Result=FTabletopScriptRuntime::Run(S.Source,Context);
    if(!Result.bSuccess) {Status=TEXT("Script stopped: ")+Result.Error;return false;}
    // Validate every command on an isolated board. Failed commands commit nothing.
    FBoardModel Candidate=*this;Candidate.OnEvent={};Candidate.bRunningScript=true;
    for(const auto& C:Result.Commands)
    {
        const FToken* Target=Candidate.Find(C.TargetId);
        if(!Target || !Candidate.CanControl(*Target)) {Status=TEXT("Script command denied; no changes applied.");return false;}
        bool OK=false;
        if(C.Kind==EScriptCommand::SetAttribute)
        {
            if(!FMath::IsFinite(C.Value) || FMath::Abs(C.Value)>1000000 || (!IsDM() && !PlayerWritable(*Target,C.Name)))
            {Status=TEXT("Script attribute denied; no changes applied.");return false;}
            OK=Candidate.SetAttribute(C.TargetId,C.Name,float(C.Value));
        }
        else OK=Candidate.SetCondition(C.TargetId,C.Name,C.Kind==EScriptCommand::AddCondition);
        if(!OK) {Status=TEXT("Script command rejected; no changes applied.");return false;}
    }
    Tokens=MoveTemp(Candidate.Tokens);Touch();
    FString Message=S.Name+TEXT(": completed");
    for(const auto& L:Result.Logs)Message+=TEXT("\n")+L;
    Log(Message.Left(4096));return true;
}
bool FBoardModel::RunScript(int32 Id,int32 SelfId)
{
    const auto* Script=Scripts.FindByPredicate([=](const FTabletopScript& S){return S.Id==Id;});
    if(bRunningScript || !Script || !Script->bEnabled || !CanReadScript(*Script) || Script->Event!=TEXT("manual"))
    {Status=TEXT("This macro is unavailable.");return false;}
    const FTabletopScript Copy=*Script;
    TGuardValue<bool> Running(bRunningScript,true);
    return ExecuteScript(Copy,SelfId==INDEX_NONE?SelectedId:SelfId,TEXT("manual"));
}
void FBoardModel::DispatchEvent(const FString& Event,int32 TokenId)
{
    if(bRunningScript || Workspace!=EWorkspace::Gameplay || !IsDMEntry()) return;
    TGuardValue<bool> Running(bRunningScript,true);
    // Every trigger receives the original event payload, even if an earlier
    // trigger changes the event token's condition list.
    FString EventCondition;
    if(Event==TEXT("condition_added"))
        if(const FToken* EventToken=Find(TokenId); EventToken && !EventToken->Conditions.IsEmpty()) EventCondition=EventToken->Conditions.Last();
    const TArray<FTabletopScript> Snapshot=Scripts;
    for(const auto& S:Snapshot)
    {
        if(!S.bEnabled || S.OwnerId!=0 || S.MapId!=ActiveMapId || S.Event!=Event || (S.TokenId && S.TokenId!=TokenId)) continue;
        const auto* T=Find(TokenId);if(!S.Condition.IsEmpty() && (!T || !T->Conditions.ContainsByPredicate([&](const FString& C){return C.Equals(S.Condition,ESearchCase::IgnoreCase);})))continue;
        FBoardModel Host=*this;Host.Role=ERole::DM;Host.OnEvent={};
        bool OK=Host.ExecuteScript(S,TokenId,Event,EventCondition);
        if(OK) {Tokens=MoveTemp(Host.Tokens);Touch();}
        TriggerLog.Insert(FString::Printf(TEXT("%s / %s / %s"),*Event,*S.Name,OK?TEXT("completed"):*Host.Status),0);
        if(TriggerLog.Num()>32)TriggerLog.SetNum(32);
        // DM event diagnostics never replace the player's visible status.
        if(IsDM())Status=Host.Status;
    }
}
void FBoardModel::EmitEvent(const FString& Event,int32 TokenId)
{
    if(bRunningScript)return;
    DispatchEvent(Event,TokenId);
    if(OnEvent)OnEvent(Event,TokenId);
}
}
