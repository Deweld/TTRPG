#include "TabletopModel.h"

namespace TTRPG::Tabletop
{
namespace
{
bool AttributeName(const FString& Name)
{
    if(Name.IsEmpty() || Name.Len()>40) return false;
    const FString Key=Name.ToLower();
    if(Key==TEXT("x") || Key==TEXT("y") || Key==TEXT("id") || Key==TEXT("name") || Key==TEXT("map_id")
        || Key==TEXT("owner_id") || Key==TEXT("attrs") || Key==TEXT("conditions")) return false;
    auto Letter=[](TCHAR C){return (C>='a' && C<='z') || (C>='A' && C<='Z') || C=='_';};
    if(!Letter(Name[0])) return false;
    for(TCHAR C:Name) if(!Letter(C) && !(C>='0' && C<='9')) return false;
    return true;
}
bool ReadValue(const FToken& T,const FString& Key,float& Out)
{
    if(Key.Equals(TEXT("hp"),ESearchCase::IgnoreCase)) Out=T.HP;
    else if(Key.Equals(TEXT("maxhp"),ESearchCase::IgnoreCase)) Out=T.Sheet.MaxHP;
    else if(Key.Equals(TEXT("defense"),ESearchCase::IgnoreCase)) Out=T.Sheet.Defense;
    else if(Key.Equals(TEXT("speed"),ESearchCase::IgnoreCase)) Out=T.Sheet.Speed;
    else { for(const auto& A:T.Attributes) if(A.Key.Equals(Key,ESearchCase::IgnoreCase)) {Out=A.Value; return true;} return false; }
    return true;
}
bool CanEditValue(const FToken& T,const FString& Key)
{
    if(Key==TEXT("hp")) return true;
    bool IsValue=false;
    for(const FStatusBar& B:T.Bars)
    {
        if(B.MaxAttribute.Equals(Key,ESearchCase::IgnoreCase)) return false;
        IsValue|=B.ValueAttribute.Equals(Key,ESearchCase::IgnoreCase);
    }
    return IsValue && Key!=TEXT("maxhp") && Key!=TEXT("defense") && Key!=TEXT("speed");
}
}
bool FBoardModel::EnterSession(ERole Entry,int32 PlayerId)
{
    if(bEnteredSession || (Entry!=ERole::DM && Entry!=ERole::Player)
        || !Players.ContainsByPredicate([PlayerId](const FLocalPlayer& P){return P.Id==PlayerId;})) return false;
    bEnteredSession=true; EntryRole=Entry; EnteredPlayerId=PlayerId; ViewingPlayerId=PlayerId;
    Workspace=EWorkspace::Gameplay; bMapEditingLocked=true; SetRole(Entry);
    return true;
}
bool FBoardModel::SetWorkspace(EWorkspace Value)
{
    if(Value<EWorkspace::MapEditor || Value>EWorkspace::Script || (Value==EWorkspace::MapEditor && !IsDM())) return false;
    Workspace=Value; Tool=ETool::Select; Touch(); return true;
}
bool FBoardModel::CanEditMap() const
{
    return IsDM() && (Workspace==EWorkspace::MapEditor || (Workspace==EWorkspace::Gameplay && !bMapEditingLocked));
}
bool FBoardModel::SetMapEditingLocked(bool bLocked)
{
    if(!IsDM()) return false;
    bMapEditingLocked=bLocked; Tool=ETool::Select;
    Log(bLocked?TEXT("Gameplay map editing locked."):TEXT("Gameplay map editing enabled.")); return true;
}
int32 FBoardModel::DuplicateMap(int32 Id)
{
    const FMapData* Source=FindMap(Id);
    if(!CanEditMap() || !Source || Maps.Num()>=16 || NextMapId>=1000000) return INDEX_NONE;
    TArray<FToken> Copies;
    for(const FToken& T:Tokens) if(T.MapId==Id && T.OwnerId==0) Copies.Add(T);
    if(Tokens.Num()+Copies.Num()>512 || NextId+Copies.Num()>=1000000) return INDEX_NONE;
    FMapData Copy=*Source; Copy.Id=NextMapId++; Copy.Name=(Source->Name+TEXT(" copy")).Left(80);
    Copy.bCombat=false; Copy.TurnOrder.Empty(); Copy.TurnIndex=0; Copy.Round=1;
    for(FToken& T:Copies) {T.Id=NextId++; T.MapId=Copy.Id; T.MovementSpent=0; T.bOutOfTurn=false;}
    Maps.Add(Copy); Tokens.Append(Copies); SwitchMap(Copy.Id);
    Log(TEXT("Map duplicated with unowned tokens. Player characters and scripts are not copied.")); return Copy.Id;
}
bool FBoardModel::DeleteMap(int32 Id)
{
    const FMapData* Existing=FindMap(Id);
    if(!CanEditMap() || !Existing || Maps.Num()<=1) return false;
    if(Existing->bCombat || Players.ContainsByPredicate([Id](const FLocalPlayer& P){return P.MapId==Id;}))
    {Status=TEXT("End combat and move assigned players off this map before deleting it."); return false;}
    Maps.RemoveAll([Id](const FMapData& M){return M.Id==Id;});
    Tokens.RemoveAll([Id](const FToken& T){return T.MapId==Id;});
    ForcedMoves.RemoveAll([Id](const FForcedMove& P){return P.MapId==Id;});
    Scripts.RemoveAll([Id](const FTabletopScript& S){return S.MapId==Id;});
    if(ActiveMapId==Id) ActiveMapId=Maps[0].Id;
    SelectedId=INDEX_NONE; Tool=ETool::Select; Log(TEXT("Map deleted.")); return true;
}
bool FBoardModel::AddMapLink(const FString& Name,const FString& Url,bool bPlayerVisible)
{
    const FString Label=Name.TrimStartAndEnd(), Address=Url.TrimStartAndEnd();
    if(!IsDM() || Label.IsEmpty() || Label.Len()>80 || Map().Links.Num()>=32 || Address.Len()>2048
        || !(Address.StartsWith(TEXT("https://"),ESearchCase::IgnoreCase) || Address.StartsWith(TEXT("http://"),ESearchCase::IgnoreCase))) return false;
    for(TCHAR C:Address) if(FChar::IsWhitespace(C) || C<32 || C=='\\') return false;
    FString Host=Address.Mid(Address.Find(TEXT("://"))+3);
    if(Host.IsEmpty() || Host.StartsWith(TEXT("/")) || Host.StartsWith(TEXT("?")) || Host.StartsWith(TEXT("#"))) return false;
    Map().Links.Add({Label,Address,bPlayerVisible}); Log(TEXT("Map link added.")); return true;
}
bool FBoardModel::RemoveMapLink(int32 Index)
{
    if(!IsDM() || !Map().Links.IsValidIndex(Index)) return false;
    Map().Links.RemoveAt(Index); Log(TEXT("Map link removed.")); return true;
}
bool FBoardModel::SetOwner(int32 TokenId,int32 OwnerId)
{
    FToken* T=Tokens.FindByPredicate([TokenId](const FToken& V){return V.Id==TokenId;});
    if(!IsDM() || !T || T->MapId!=ActiveMapId || OwnerId<0) return false;
    const FLocalPlayer* P=Players.FindByPredicate([OwnerId](const FLocalPlayer& V){return V.Id==OwnerId;});
    if(OwnerId && (!P || P->MapId!=T->MapId)) {Status=TEXT("Owner must be assigned to the token's map."); return false;}
    if(T->OwnerId==OwnerId) return true;
    // Preserve the old owner's source as a manual map macro, without a token binding.
    for(FTabletopScript& S:Scripts) if(S.TokenId==TokenId && S.OwnerId && S.OwnerId!=OwnerId) S.TokenId=0;
    T->OwnerId=OwnerId; T->bOwnedByPlayer=OwnerId==1; AdvanceTurnGeneration();
    Log(TEXT("Token ownership updated.")); return true;
}
bool FBoardModel::CaptureTokenDefaults(int32 TokenId)
{
    const FToken* T=Find(TokenId); if(!IsDM() || !T || !IsVisible(*T)) return false;
    TokenDefaults.Sheet=T->Sheet; TokenDefaults.Attributes=T->Attributes; TokenDefaults.Bars=T->Bars;
    TokenDefaults.VisionRange=T->VisionRange; TokenDefaults.VisionAngle=T->VisionAngle; TokenDefaults.bDarkvision=T->bDarkvision;
    Log(TEXT("Defaults captured. Existing token instances retain their properties.")); return true;
}
bool FBoardModel::ApplyTokenDefaults(int32 TokenId)
{
    FToken* T=Tokens.FindByPredicate([TokenId](const FToken& V){return V.Id==TokenId;});
    if(!IsDM() || !T || !IsVisible(*T)) return false;
    T->Sheet=TokenDefaults.Sheet; T->Attributes=TokenDefaults.Attributes; T->Bars=TokenDefaults.Bars;
    T->VisionRange=TokenDefaults.VisionRange; T->VisionAngle=TokenDefaults.VisionAngle; T->bDarkvision=TokenDefaults.bDarkvision;
    T->HP=FMath::Min(T->HP,T->Sheet.MaxHP); Log(TEXT("Defaults applied to this instance.")); return true;
}
bool FBoardModel::GetAttribute(int32 TokenId,const FString& Attribute,float& OutValue) const
{
    const FToken* T=Find(TokenId);
    return T && IsVisible(*T) && AttributeName(Attribute) && ReadValue(*T,Attribute,OutValue);
}
bool FBoardModel::SetAttribute(int32 TokenId,const FString& Attribute,float Value)
{
    FToken* T=Tokens.FindByPredicate([TokenId](const FToken& V){return V.Id==TokenId;});
    const FString Key=Attribute.ToLower();
    if(!T || !CanControl(*T) || !AttributeName(Key) || !FMath::IsFinite(Value) || FMath::Abs(Value)>1000000) return false;
    if(!IsDM() && !CanEditValue(*T,Key)) {Status=TEXT("Only the DM can define attributes and edit resource limits."); return false;}
    if(Key==TEXT("hp")) {if(Value!=FMath::FloorToFloat(Value) || Value<0 || Value>T->Sheet.MaxHP) return false; T->HP=int32(Value);}
    else if(Key==TEXT("maxhp")) {if(!IsDM() || Value!=FMath::FloorToFloat(Value) || Value<1 || Value>9999) return false; T->Sheet.MaxHP=int32(Value); T->HP=FMath::Min(T->HP,T->Sheet.MaxHP);}
    else if(Key==TEXT("defense")) {if(!IsDM() || Value!=FMath::FloorToFloat(Value) || Value<0 || Value>999) return false; T->Sheet.Defense=int32(Value);}
    else if(Key==TEXT("speed")) {if(!IsDM() || Value<0 || Value>1000) return false; T->Sheet.Speed=Value;}
    else
    {
        FString ExistingKey;
        for(const auto& A:T->Attributes) if(A.Key.Equals(Key,ESearchCase::IgnoreCase)) {ExistingKey=A.Key; break;}
        if(ExistingKey.IsEmpty() && (!IsDM() || T->Attributes.Num()>=32)) return false;
        if(!ExistingKey.IsEmpty()) T->Attributes.Remove(ExistingKey);
        T->Attributes.Add(Key,Value);
    }
    Log(TEXT("Token attribute updated.")); return true;
}
bool FBoardModel::SetCondition(int32 TokenId,const FString& Condition,bool bPresent)
{
    FToken* T=Tokens.FindByPredicate([TokenId](const FToken& V){return V.Id==TokenId;});
    FString Name=Condition.TrimStartAndEnd();
    if(!T || !CanControl(*T) || Name.IsEmpty() || Name.Len()>60) return false;
    int32 Found=T->Conditions.IndexOfByPredicate([&](const FString& V){return V.Equals(Name,ESearchCase::IgnoreCase);});
    if(bPresent)
    {
        if(Found!=INDEX_NONE) return true;
        if(T->Conditions.Num()>=16) return false;
        T->Conditions.Add(Name); Log(TEXT("Condition added.")); EmitEvent(TEXT("condition_added"),TokenId);
    }
    else {if(Found==INDEX_NONE) return true; T->Conditions.RemoveAt(Found); Log(TEXT("Condition removed."));}
    return true;
}
bool FBoardModel::SetStatusBars(int32 TokenId,const TArray<FStatusBar>& Bars)
{
    FToken* T=Tokens.FindByPredicate([TokenId](const FToken& V){return V.Id==TokenId;});
    if(!IsDM() || !T || !IsVisible(*T) || Bars.Num()>4) return false;
    for(const FStatusBar& B:Bars)
    {
        float V=0,Maximum=0;
        if(B.Label.TrimStartAndEnd().IsEmpty() || B.Label.Len()>40 || !AttributeName(B.ValueAttribute) || !AttributeName(B.MaxAttribute)
            || !ReadValue(*T,B.ValueAttribute,V) || !ReadValue(*T,B.MaxAttribute,Maximum)) return false;
    }
    T->Bars=Bars; Log(TEXT("Status bars assigned to token attributes.")); return true;
}
bool FBoardModel::SetInitiative(int32 TokenId,float Value)
{
    FToken* T=Tokens.FindByPredicate([TokenId](const FToken& V){return V.Id==TokenId;});
    if(!IsDM() || !T || !IsVisible(*T) || T->Kind==ETokenKind::Prop || !FMath::IsFinite(Value) || Value<-1000 || Value>1000) return false;
    T->Initiative=Value;
    if(Map().bCombat)
    {
        const int32 Current=ActiveTurnId();
        Map().TurnOrder.Sort([this](int32 A,int32 B){const FToken* TA=Find(A); const FToken* TB=Find(B); return TA->Initiative==TB->Initiative?A<B:TA->Initiative>TB->Initiative;});
        Map().TurnIndex=Map().TurnOrder.IndexOfByKey(Current); AdvanceTurnGeneration();
    }
    Log(TEXT("Initiative updated. An active combat keeps its current actor.")); return true;
}
void FBoardModel::AdvanceTurnGeneration()
{
    MapTurnGenerations.Add(ActiveMapId,++TurnGeneration);
    for(FForcedMove& P:ForcedMoves) if(P.MapId==ActiveMapId && P.State==EMoveProposalState::Pending) P.State=EMoveProposalState::Stale;
}
bool FBoardModel::CanRequestForcedMove(const FToken& Target) const
{
    const FToken* Actor=Find(ActiveTurnId());
    return Role==ERole::Player && (!bEnteredSession || Workspace==EWorkspace::Gameplay) && Map().bCombat && Actor && Actor->OwnerId==ViewingPlayerId && IsVisible(*Actor)
        && Actor->Id!=Target.Id && IsVisible(Target) && Target.MapId==Actor->MapId;
}
int32 FBoardModel::RequestForcedMove(int32 TargetId,FIntPoint Cell)
{
    const FToken* Target=Find(TargetId);
    if(!Target || !CanRequestForcedMove(*Target) || Target->Cell==Cell || !IsCellVisible(Cell)
        || !IsAvailable(Cell,TargetId) || PathDistance(*Target,Cell)<0 || NextProposalId>=1000000) return INDEX_NONE;
    const FToken* Actor=Find(ActiveTurnId());
    if(ForcedMoves.ContainsByPredicate([&](const FForcedMove& P){return P.State==EMoveProposalState::Pending && P.PlayerId==ViewingPlayerId && P.TargetId==TargetId;}))
    {Status=TEXT("A request for this token is already awaiting the DM."); return INDEX_NONE;}
    if(ForcedMoves.Num()>=64)
    {
        int32 Decided=ForcedMoves.IndexOfByPredicate([](const FForcedMove& P){return P.State!=EMoveProposalState::Pending;});
        if(Decided==INDEX_NONE) return INDEX_NONE;
        ForcedMoves.RemoveAt(Decided);
    }
    FForcedMove P; P.Id=NextProposalId++; P.MapId=ActiveMapId; P.ActorId=Actor->Id; P.TargetId=TargetId;
    P.PlayerId=ViewingPlayerId; P.TargetOwnerId=Target->OwnerId; P.ActorCell=Actor->Cell; P.TargetCell=Target->Cell;
    P.Destination=Cell; P.TurnGeneration=MapTurnGenerations.FindRef(ActiveMapId);
    ForcedMoves.Add(P); Log(TEXT("Forced movement submitted to the DM. The target has not moved.")); return P.Id;
}
bool FBoardModel::ResolveForcedMove(int32 ProposalId,bool bApprove)
{
    if(!IsDM()) return false;
    FForcedMove* P=ForcedMoves.FindByPredicate([ProposalId](const FForcedMove& V){return V.Id==ProposalId;});
    if(!P || P->State!=EMoveProposalState::Pending) return false;
    if(!bApprove) {P->State=EMoveProposalState::Rejected; Log(TEXT("Movement request rejected.")); return true;}
    if(P->MapId!=ActiveMapId) {Status=TEXT("Open the request's map before approving it."); return false;}
    const FToken* Actor=Find(P->ActorId); FToken* Target=Tokens.FindByPredicate([P](const FToken& V){return V.Id==P->TargetId;});
    const FLocalPlayer* Author=Players.FindByPredicate([P](const FLocalPlayer& V){return V.Id==P->PlayerId;});
    bool Valid=P->MapId==ActiveMapId && Map().bCombat && P->TurnGeneration==MapTurnGenerations.FindRef(P->MapId) && ActiveTurnId()==P->ActorId
        && Actor && Target && Author && Author->MapId==P->MapId && Actor->MapId==P->MapId && Target->MapId==P->MapId
        && Actor->OwnerId==P->PlayerId && Target->OwnerId==P->TargetOwnerId && Actor->Cell==P->ActorCell && Target->Cell==P->TargetCell
        && !Actor->bHidden && !Target->bHidden && IsAvailable(P->Destination,P->TargetId);
    if(Valid)
    {
        // Re-evaluate the proposer's current vision instead of trusting visibility when submitted.
        FBoardModel Preview=*this; Preview.Role=ERole::Player; Preview.ViewingPlayerId=P->PlayerId;
        Preview.Touch();
        Valid=Preview.IsCellVisible(Target->Cell) && Preview.IsCellVisible(P->Destination) && PathDistance(*Target,P->Destination)>=0;
    }
    if(!Valid) {P->State=EMoveProposalState::Stale; Log(TEXT("Movement request is stale or no longer permitted. Ask the player to resubmit.")); return false;}
    Target->Cell=P->Destination; P->State=EMoveProposalState::Approved;
    const int32 TargetId=Target->Id;
    Log(TEXT("DM approved forced movement. The target's movement allowance is unchanged.")); EmitEvent(TEXT("move"),TargetId); return true;
}
}
