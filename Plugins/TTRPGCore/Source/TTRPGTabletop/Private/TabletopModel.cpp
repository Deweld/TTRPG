#include "TabletopModel.h"

namespace TTRPG::Tabletop
{
bool FMapData::Contains(FIntPoint C) const { return C.X >= 0 && C.Y >= 0 && C.X < Width && C.Y < Height; }
FVector2D FMapData::Extent() const
{
    return Grid == EGrid::Square ? FVector2D(Width, Height) : FVector2D(Width + (Height > 1 ? .5 : 0), (Height - 1) * .8660254038 + 1.1547005384);
}
FVector2D FMapData::Center(FIntPoint C) const
{
    return Grid == EGrid::Square ? FVector2D(C.X + .5, C.Y + .5) : FVector2D(C.X + .5 + .5 * (C.Y & 1), C.Y * .8660254038 + .5773502692);
}
FIntPoint FMapData::CellFromPoint(FVector2D P) const
{
    if (Grid == EGrid::Square) return {FMath::FloorToInt32(P.X), FMath::FloorToInt32(P.Y)};
    const double R = (P.Y - .5773502692) / .8660254038;
    const double Q = P.X - .5 - R * .5, S = -Q - R;
    int32 X = FMath::RoundToInt(Q), Z = FMath::RoundToInt(R), Y = FMath::RoundToInt(S);
    const double DX = FMath::Abs(X - Q), DZ = FMath::Abs(Z - R), DY = FMath::Abs(Y - S);
    if (DX > DY && DX > DZ) X = -Y - Z; else if (DZ > DY) Z = -X - Y;
    return {X + (Z - (Z & 1)) / 2, Z};
}
TArray<FIntPoint> FMapData::Neighbors(FIntPoint C) const
{
    TArray<FIntPoint> Out;
    if (Grid == EGrid::Square)
    {
        for (int32 Y = -1; Y <= 1; ++Y) for (int32 X = -1; X <= 1; ++X)
            if (X || Y) Out.Add(C + FIntPoint(X, Y));
    }
    else
    {
        const int32 O = C.Y & 1;
        Out = {{C.X-1,C.Y},{C.X+1,C.Y},{C.X-1+O,C.Y-1},{C.X+O,C.Y-1},{C.X-1+O,C.Y+1},{C.X+O,C.Y+1}};
    }
    return Out;
}
FBoardModel::FBoardModel()
{
    Maps.Add(FMapData());
    FMapData Cave; Cave.Id=2; Cave.Name=TEXT("Lantern Cavern"); Cave.Width=16; Cave.Height=12;
    Cave.Grid=EGrid::Hex; Cave.bVision=true; Cave.bDarkness=true;
    Cave.Lights={{{4,5},25},{{11,7},20}};
    for (int32 Y=1; Y<10; ++Y) if (Y!=6) Cave.Walls.Add({8,Y});
    Maps.Add(Cave);
    Players={{1,TEXT("Mira's player"),1},{2,TEXT("Rowan's player"),1}};
    auto& W=Map().Walls;
    for (int32 X=1; X<23; ++X) { W.Add({X,1}); W.Add({X,16}); }
    for (int32 Y=2; Y<16; ++Y) { W.Add({1,Y}); W.Add({22,Y}); }
    W.Remove({11,16}); W.Remove({12,16});
    for (int32 X=5; X<=10; ++X) if(X!=7) W.Add({X,5});
    for (int32 Y=6; Y<=8; ++Y) { W.Add({5,Y}); W.Add({10,Y}); }
    for (int32 X=14; X<=19; ++X) { W.Add({X,4}); if(X!=16 && X!=17) W.Add({X,9}); }
    for (int32 Y=5; Y<=8; ++Y) { W.Add({14,Y}); W.Add({19,Y}); }
    Tokens.Add({NextId++,TEXT("Mira"),{6,12},ETokenKind::Hero,true,false,24});
    Tokens.Add({NextId++,TEXT("Rowan"),{9,12},ETokenKind::Hero,false,false,20});
    Tokens.Add({NextId++,TEXT("Sentinel"),{16,7},ETokenKind::NPC,false,false,18});
    Tokens.Add({NextId++,TEXT("Lookout"),{20,5},ETokenKind::NPC,false,true,12});
    Tokens.Add({NextId++,TEXT("Supply crate"),{8,7},ETokenKind::Prop,false,false,1});
    Tokens.Add({NextId++,TEXT("Old chest"),{17,12},ETokenKind::Prop,false,false,1});
    Tokens[0].OwnerId=1; Tokens[1].OwnerId=2;
    for(FToken& T:Tokens) T.Sheet.MaxHP=T.HP;
    Map().Lights={{{7,10},30},{{16,6},20}};
    SelectedId=1; Activity.Add(TEXT("Two maps ready. DM controls player assignments."));
}
FMapData& FBoardModel::Map() { return *Maps.FindByPredicate([this](const FMapData& M){return M.Id==ActiveMapId;}); }
const FMapData& FBoardModel::Map() const { return *FindMap(ActiveMapId); }
const FMapData* FBoardModel::FindMap(int32 Id) const { return Maps.FindByPredicate([Id](const FMapData& M){return M.Id==Id;}); }
const FLocalPlayer* FBoardModel::Player() const { return Players.FindByPredicate([this](const FLocalPlayer& P){return P.Id==ViewingPlayerId;}); }
const FToken* FBoardModel::Find(int32 Id) const { return Tokens.FindByPredicate([Id](const FToken& T){return T.Id==Id;}); }
FToken* FBoardModel::Selected() { return Tokens.FindByPredicate([this](const FToken& T){return T.Id==SelectedId && IsVisible(T);}); }
const FToken* FBoardModel::Selected() const { const FToken* T=Find(SelectedId); return T && IsVisible(*T)?T:nullptr; }
void FBoardModel::Touch() { ++Revision; }
bool FBoardModel::IsInside(FIntPoint C) const { return Map().Contains(C); }
bool FBoardModel::IsVisible(const FToken& T) const { return T.MapId==ActiveMapId && (Role==ERole::DM || (!T.bHidden && IsCellVisible(T.Cell))); }
bool FBoardModel::CanControl(const FToken& T) const { return IsVisible(T) && (Role==ERole::DM || T.OwnerId==ViewingPlayerId); }
const FToken* FBoardModel::TokenAt(FIntPoint C,bool VisibleOnly) const
{
    return Tokens.FindByPredicate([&](const FToken& T){return T.MapId==ActiveMapId && T.Cell==C && (!VisibleOnly || IsVisible(T));});
}
bool FBoardModel::IsAvailable(FIntPoint C,int32 IgnoreId) const
{
    const FToken* T=TokenAt(C,false); return IsInside(C) && !Map().Walls.Contains(C) && (!T || T->Id==IgnoreId);
}
void FBoardModel::Log(const FString& Message) { Status=Message; Activity.Insert(Message,0); if(Activity.Num()>6) Activity.SetNum(6); Touch(); }
void FBoardModel::SetRole(ERole NewRole)
{
    Role=NewRole; Tool=ETool::Select; SelectedId=INDEX_NONE;
    if(Role==ERole::Player && Player()) ActiveMapId=Player()->MapId;
    Touch();
    for(const FToken& T:Tokens) if(IsVisible(T) && T.OwnerId==ViewingPlayerId) { SelectedId=T.Id; break; }
    Activity.Empty(); Log(Role==ERole::DM?TEXT("DM view: edit maps and assign players independently."):TEXT("Player view: your assigned map and visible characters only."));
}
void FBoardModel::SetPlayer(int32 Id)
{
    if(!Players.ContainsByPredicate([Id](const FLocalPlayer& P){return P.Id==Id;})) return;
    ViewingPlayerId=Id; SetRole(Role);
}
bool FBoardModel::SwitchMap(int32 Id)
{
    if(!FindMap(Id) || (Role!=ERole::DM && (!Player() || Player()->MapId!=Id))) return false;
    ActiveMapId=Id; SelectedId=INDEX_NONE; Tool=ETool::Select; Activity.Empty(); Log(TEXT("Map opened. Player assignments are unchanged.")); return true;
}
int32 FBoardModel::AddMap(const FString& Name,int32 Width,int32 Height,EGrid Grid)
{
    if(Role!=ERole::DM || Maps.Num()>=16 || Width<4 || Width>64 || Height<4 || Height>64 || Name.TrimStartAndEnd().IsEmpty()) return INDEX_NONE;
    FMapData M; M.Id=NextMapId++; M.Name=Name.Left(80); M.Width=Width; M.Height=Height; M.Grid=Grid;
    Maps.Add(M); SwitchMap(M.Id); return M.Id;
}
bool FBoardModel::ResizeMap(int32 Width,int32 Height)
{
    if(Role!=ERole::DM || Width<4 || Width>64 || Height<4 || Height>64) return false;
    auto Fits=[=](FIntPoint C){return C.X<Width && C.Y<Height;};
    for(FIntPoint C:Map().Walls) if(!Fits(C)) {Status=TEXT("Resize would cut off walls. Clear them first."); return false;}
    for(const FMapLight& L:Map().Lights) if(!Fits(L.Cell)) {Status=TEXT("Resize would cut off lights."); return false;}
    for(const FToken& T:Tokens) if(T.MapId==ActiveMapId && !Fits(T.Cell)) {Status=TEXT("Resize would cut off tokens."); return false;}
    Map().Width=Width; Map().Height=Height; Log(TEXT("Map size updated.")); return true;
}
bool FBoardModel::AssignPlayer(int32 Id,int32 MapId)
{
    FLocalPlayer* P=Players.FindByPredicate([Id](const FLocalPlayer& V){return V.Id==Id;});
    const FMapData* Dest=FindMap(MapId);
    if(Role!=ERole::DM || !P || !Dest) return false;
    if(P->MapId==MapId) return true;
    if(FindMap(P->MapId)->bCombat || Dest->bCombat) {Status=TEXT("End combat on both maps before transferring this player."); return false;}
    TArray<int32> Owned;
    for(int32 I=0; I<Tokens.Num(); ++I) if(Tokens[I].OwnerId==Id) Owned.Add(I);
    TArray<FIntPoint> Free;
    for(int32 Y=0; Y<Dest->Height && Free.Num()<Owned.Num(); ++Y) for(int32 X=0; X<Dest->Width && Free.Num()<Owned.Num(); ++X)
    {
        FIntPoint C(X,Y);
        if(!Dest->Walls.Contains(C) && !Tokens.ContainsByPredicate([&](const FToken& T){return T.MapId==MapId && T.Cell==C;})) Free.Add(C);
    }
    if(Free.Num()<Owned.Num()) {Status=TEXT("Destination has too few open cells. No changes made."); return false;}
    for(int32 I=0; I<Owned.Num(); ++I) {FToken& T=Tokens[Owned[I]]; T.MapId=MapId; T.Cell=Free[I]; T.MovementSpent=0;}
    P->MapId=MapId; SelectedId=INDEX_NONE;
    Log(FString::Printf(TEXT("%s assigned to %s."),*P->Name,*Dest->Name)); return true;
}
void FBoardModel::Select(int32 Id)
{
    const FToken* T=Find(Id); SelectedId=T && IsVisible(*T)?Id:INDEX_NONE;
    Status=T && CanControl(*T)?TEXT("Drag to move. Sheet speed and turn rules apply in player view."):TEXT("View only / another participant.");
}
float FBoardModel::PathDistance(const FToken& Token,FIntPoint Goal) const
{
    if(!IsAvailable(Goal,Token.Id)) return -1;
    struct FNode { FIntPoint Cell; float Score; bool operator<(const FNode& O) const {return Score<O.Score;} };
    TArray<FNode> Open; TMap<FIntPoint,float> Cost; TSet<FIntPoint> Closed;
    auto H=[&](FIntPoint C){return float((Map().Center(C)-Map().Center(Goal)).Size())*Map().UnitsPerCell;};
    Cost.Add(Token.Cell,0); Open.HeapPush({Token.Cell,H(Token.Cell)});
    while(!Open.IsEmpty())
    {
        FNode Node; Open.HeapPop(Node); if(Closed.Contains(Node.Cell)) continue;
        if(Node.Cell==Goal) return Cost[Node.Cell]; Closed.Add(Node.Cell);
        for(FIntPoint N:Map().Neighbors(Node.Cell))
        {
            if(!IsAvailable(N,Token.Id) || Closed.Contains(N)) continue;
            FIntPoint D=N-Node.Cell;
            if(Map().Grid==EGrid::Square && D.X && D.Y && (!IsAvailable(Node.Cell+FIntPoint(D.X,0),Token.Id) || !IsAvailable(Node.Cell+FIntPoint(0,D.Y),Token.Id))) continue;
            float Step=Map().UnitsPerCell*(Map().Grid==EGrid::Square && D.X && D.Y?1.41421356f:1.f);
            float NewCost=Cost[Node.Cell]+Step; const float* Old=Cost.Find(N);
            if(!Old || NewCost<*Old) {Cost.Add(N,NewCost); Open.HeapPush({N,NewCost+H(N)});}
        }
    }
    return -1;
}
int32 FBoardModel::ActiveTurnId() const {return Map().bCombat && Map().TurnOrder.IsValidIndex(Map().TurnIndex)?Map().TurnOrder[Map().TurnIndex]:INDEX_NONE;}
float FBoardModel::RemainingMovement(const FToken& T) const
{
    float Limit=Map().bCombat?T.Sheet.Speed:Map().bLimitMovement?Map().MovementLimit:-1;
    if(Map().bCombat && Map().bLimitMovement) Limit=FMath::Min(Limit,Map().MovementLimit);
    return Limit<0?-1:FMath::Max(0.f,Limit-T.MovementSpent);
}
bool FBoardModel::Move(int32 Id,FIntPoint Cell)
{
    FToken* T=Tokens.FindByPredicate([Id](const FToken& V){return V.Id==Id;});
    if(!T || !CanControl(*T)) {Status=TEXT("You can only move a token you control on this map."); return false;}
    if(!IsAvailable(Cell,Id) || (Role==ERole::Player && !IsCellVisible(Cell))) {Status=TEXT("That destination is unavailable."); return false;}
    if(T->Cell==Cell) return true;
    if(Role==ERole::Player)
    {
        if(Map().bCombat && ActiveTurnId()!=Id && !T->bOutOfTurn) {Status=TEXT("Wait for your turn, or ask the DM for a movement exception."); return false;}
        float Cost=PathDistance(*T,Cell), Remaining=RemainingMovement(*T);
        if(Cost<0) {Status=TEXT("There is no open path to that destination."); return false;}
        if(Remaining>=0 && Cost>Remaining+.001f) {Status=FString::Printf(TEXT("Movement budget exceeded. %.1f units remain."),Remaining); return false;}
        if(Remaining>=0) T->MovementSpent+=Cost;
    }
    T->Cell=Cell; Touch(); Log(FString::Printf(TEXT("%s moved to column %d, row %d."),*T->Name,Cell.X+1,Cell.Y+1)); return true;
}
void FBoardModel::ToggleCombat()
{
    if(Role!=ERole::DM) return;
    auto& M=Map(); M.bCombat=!M.bCombat; M.TurnOrder.Empty(); M.TurnIndex=0; M.Round=1;
    for(FToken& T:Tokens) if(T.MapId==ActiveMapId) {T.MovementSpent=0; if(M.bCombat && T.Kind!=ETokenKind::Prop) M.TurnOrder.Add(T.Id);}
    if(M.TurnOrder.IsEmpty()) M.bCombat=false;
    Log(M.bCombat?TEXT("Combat started. Turn order follows the roster."):TEXT("Combat ended. Movement allowances reset."));
}
void FBoardModel::NextTurn()
{
    if(Role!=ERole::DM || !Map().bCombat || Map().TurnOrder.IsEmpty()) return;
    Map().TurnIndex=(Map().TurnIndex+1)%Map().TurnOrder.Num(); if(!Map().TurnIndex) ++Map().Round;
    for(FToken& T:Tokens) if(T.Id==ActiveTurnId()) T.MovementSpent=0;
    Log(TEXT("Next turn. Active character's movement refreshed."));
}
void FBoardModel::ResetMovement()
{
    if(Role!=ERole::DM) return;
    for(FToken& T:Tokens) if(T.MapId==ActiveMapId) T.MovementSpent=0;
    Log(TEXT("DM refreshed movement allowances on this map."));
}
bool FBoardModel::Place(ETokenKind Kind,FIntPoint Cell)
{
    if(Role!=ERole::DM || !IsAvailable(Cell) || NextId>=MAX_int32 || Tokens.Num()>=512) return false;
    if(Kind==ETokenKind::Hero && (!Player() || Player()->MapId!=ActiveMapId)) {Status=TEXT("Assign the selected player to this map before placing their character."); return false;}
    FString Name=Kind==ETokenKind::Hero?TEXT("Adventurer"):Kind==ETokenKind::NPC?TEXT("Scout"):TEXT("Crate");
    FToken T; T.Id=NextId++; T.Name=FString::Printf(TEXT("%s %d"),*Name,T.Id); T.Cell=Cell; T.Kind=Kind;
    T.MapId=ActiveMapId; T.OwnerId=Kind==ETokenKind::Hero?ViewingPlayerId:0; T.bOwnedByPlayer=T.OwnerId==1;
    T.HP=Kind==ETokenKind::Prop?1:20; T.Sheet.MaxHP=T.HP; Tokens.Add(T); SelectedId=T.Id;
    if(Map().bCombat && Kind!=ETokenKind::Prop) Map().TurnOrder.Add(T.Id);
    Log(TEXT("Token placed on the active map.")); return true;
}
bool FBoardModel::PaintWall(FIntPoint Cell,bool Add)
{
    if(Role!=ERole::DM || !IsInside(Cell) || (Add && (TokenAt(Cell,false) || Map().Lights.ContainsByPredicate([&](const FMapLight& L){return L.Cell==Cell;})))) return false;
    if(Add) Map().Walls.Add(Cell); else Map().Walls.Remove(Cell);
    Status=Add?TEXT("Wall placed: blocks movement and line of sight."):TEXT("Wall removed."); Touch(); return true;
}
bool FBoardModel::PaintStroke(FIntPoint From,FIntPoint To,bool Add)
{
    if(Role!=ERole::DM || !IsInside(From) || !IsInside(To)) return false;
    FVector2D A=Map().Center(From), B=Map().Center(To); int32 Steps=FMath::Max(1,FMath::CeilToInt((B-A).Size()*3)); bool Changed=false;
    for(int32 I=0; I<=Steps; ++I) Changed|=PaintWall(Map().CellFromPoint(FMath::Lerp(A,B,double(I)/Steps)),Add);
    return Changed;
}
bool FBoardModel::PlaceLight(FIntPoint Cell,float Range,bool Remove)
{
    if(Role!=ERole::DM || !IsInside(Cell)) return false;
    if(Remove) Map().Lights.RemoveAll([&](const FMapLight& L){return L.Cell==Cell;});
    else
    {
        if(Map().Walls.Contains(Cell) || !FMath::IsFinite(Range) || Range<1 || Range>500) return false;
        FMapLight* L=Map().Lights.FindByPredicate([&](const FMapLight& V){return V.Cell==Cell;});
        if(L) L->Range=Range; else if(Map().Lights.Num()<32) Map().Lights.Add({Cell,Range}); else return false;
    }
    Log(Remove?TEXT("Light removed."):TEXT("Light placed. Walls block illumination.")); return true;
}
bool FBoardModel::ToggleHidden()
{
    FToken* T=Selected(); if(Role!=ERole::DM || !T) return false; T->bHidden=!T->bHidden; Log(TEXT("Token visibility updated.")); return true;
}
bool FBoardModel::ChangeHP(int32 Delta)
{
    FToken* T=Selected(); if(!T || !CanControl(*T) || T->Kind==ETokenKind::Prop) return false;
    T->HP=FMath::Clamp(T->HP+FMath::Clamp(Delta,-1,1),0,T->Sheet.MaxHP); Log(TEXT("HP updated.")); return true;
}
bool FBoardModel::UpdateSheet(const FCharacterSheet& Sheet,const FString& Name,int32 HP)
{
    FToken* T=Selected();
    if(!T || !CanControl(*T) || T->Kind==ETokenKind::Prop || Name.TrimStartAndEnd().IsEmpty() || Name.Len()>80 || Sheet.Notes.Len()>4096
        || Sheet.MaxHP<1 || Sheet.MaxHP>9999 || HP<0 || HP>Sheet.MaxHP || Sheet.Defense<0 || Sheet.Defense>999 || !FMath::IsFinite(Sheet.Speed) || Sheet.Speed<0 || Sheet.Speed>1000) return false;
    if(Role==ERole::Player && (Sheet.MaxHP!=T->Sheet.MaxHP || Sheet.Speed!=T->Sheet.Speed || Sheet.Defense!=T->Sheet.Defense)) {Status=TEXT("Max HP, defense, and speed are DM-controlled."); return false;}
    T->Sheet=Sheet; T->Name=Name; T->HP=HP; Log(TEXT("Character sheet saved.")); return true;
}
void FBoardModel::RollD20()
{
    const FToken* T=Selected(); if(!T || !CanControl(*T) || T->Kind==ETokenKind::Prop) return;
    Log(FString::Printf(TEXT("%s rolled d20: %d"),*T->Name,FMath::RandRange(1,20)));
}
bool FBoardModel::HasSight(const FMapData& M,FIntPoint From,FIntPoint To) const
{
    if(!M.Contains(From) || !M.Contains(To)) return false; if(From==To) return true;
    FVector2D A=M.Center(From), B=M.Center(To); int32 Steps=FMath::Max(1,FMath::CeilToInt((B-A).Size()*8)); FIntPoint Previous=From;
    for(int32 I=1; I<=Steps; ++I)
    {
        FIntPoint C=M.CellFromPoint(FMath::Lerp(A,B,double(I)/Steps)); if(C==Previous) continue;
        FIntPoint D=C-Previous;
        if(M.Grid==EGrid::Square && D.X && D.Y && (M.Walls.Contains(Previous+FIntPoint(D.X,0)) || M.Walls.Contains(Previous+FIntPoint(0,D.Y)))) return false;
        if(C==To) return true; if(M.Walls.Contains(C)) return false; Previous=C;
    }
    return true;
}
bool FBoardModel::IsLit(const FMapData& M,FIntPoint C) const
{
    if(!M.bDarkness) return true;
    // Reuse wall-occluded illumination across Slate frames and token vision checks.
    if(LightRevision!=Revision || LightMap!=M.Id) {LitCells.Empty(); LightRevision=Revision; LightMap=M.Id;}
    if(const bool* Cached=LitCells.Find(C)) return *Cached;
    bool Lit=false;
    for(const FMapLight& L:M.Lights) if((M.Center(C)-M.Center(L.Cell)).Size()*M.UnitsPerCell<=L.Range+.001 && HasSight(M,L.Cell,C)) {Lit=true; break;}
    LitCells.Add(C,Lit); return Lit;
}
void FBoardModel::RefreshVision() const
{
    if(VisionRevision==Revision && VisionMap==ActiveMapId && VisionPlayer==ViewingPlayerId) return;
    VisibleCells.Empty(); VisionRevision=Revision; VisionMap=ActiveMapId; VisionPlayer=ViewingPlayerId;
    const FMapData& M=Map();
    for(int32 Y=0; Y<M.Height; ++Y) for(int32 X=0; X<M.Width; ++X)
    {
        FIntPoint C(X,Y);
        for(const FToken& T:Tokens)
        {
            if(T.MapId!=ActiveMapId || T.OwnerId!=ViewingPlayerId || T.bHidden || T.Kind==ETokenKind::Prop) continue;
            if(C==T.Cell) {VisibleCells.Add(C); break;}
            FVector2D D=M.Center(C)-M.Center(T.Cell); if(D.Size()*M.UnitsPerCell>T.VisionRange+.001) continue;
            float Angle=FMath::RadiansToDegrees(FMath::Atan2(float(D.Y),float(D.X)));
            if(T.VisionAngle<360 && FMath::Abs(FMath::FindDeltaAngleDegrees(T.Facing,Angle))>T.VisionAngle*.5f+.001f) continue;
            if(HasSight(M,T.Cell,C) && (T.bDarkvision || IsLit(M,C))) {VisibleCells.Add(C); break;}
        }
    }
}
bool FBoardModel::IsCellVisible(FIntPoint C) const
{
    if(!IsInside(C)) return false; if(Role==ERole::DM) return true;
    if(!Player() || Player()->MapId!=ActiveMapId) return false; if(!Map().bVision) return true;
    RefreshVision(); return VisibleCells.Contains(C);
}
}
