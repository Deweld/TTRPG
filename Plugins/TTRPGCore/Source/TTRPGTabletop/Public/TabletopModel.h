#pragma once
#include "CoreMinimal.h"
#include "TabletopScript.h"

namespace TTRPG::Tabletop
{
enum class ERole : uint8 { DM, Player };
enum class ETool : uint8 { Select, Hero, NPC, Prop, Wall, Erase, Light, EraseLight };
enum class ETokenKind : uint8 { Hero, NPC, Prop };
enum class EGrid : uint8 { Square, Hex };
enum class EWorkspace : uint8 { MapEditor, Gameplay, TokenEdit, Script };
struct TTRPGTABLETOP_API FCharacterSheet
{
    int32 MaxHP = 24;
    int32 Defense = 10;
    float Speed = 30;
    FString Notes;
};
struct TTRPGTABLETOP_API FStatusBar
{
    FString Label = TEXT("HP");
    FString ValueAttribute = TEXT("hp");
    FString MaxAttribute = TEXT("maxHP");
};
struct TTRPGTABLETOP_API FTokenDefaults
{
    FCharacterSheet Sheet;
    TMap<FString, float> Attributes;
    TArray<FStatusBar> Bars = {FStatusBar()};
    float VisionRange = 60;
    float VisionAngle = 360;
    bool bDarkvision = false;
};
struct TTRPGTABLETOP_API FToken
{
    int32 Id = 0;
    FString Name;
    FIntPoint Cell;
    ETokenKind Kind = ETokenKind::Hero;
    bool bOwnedByPlayer = false; // v1 compatibility; OwnerId is authoritative in v2.
    bool bHidden = false;
    int32 HP = 20;
    int32 MapId = 1;
    int32 OwnerId = 0;
    FCharacterSheet Sheet;
    float VisionRange = 60;
    float VisionAngle = 360;
    float Facing = 0;
    bool bDarkvision = false;
    bool bOutOfTurn = false;
    float MovementSpent = 0;
    TMap<FString, float> Attributes;
    TArray<FString> Conditions;
    TArray<FStatusBar> Bars = {FStatusBar()};
    float Initiative = 0;
};
struct TTRPGTABLETOP_API FMapLight { FIntPoint Cell; float Range = 30; };
struct TTRPGTABLETOP_API FMapLink { FString Name; FString Url; bool bPlayerVisible = false; };
struct TTRPGTABLETOP_API FMapData
{
    int32 Id = 1;
    FString Name = TEXT("The Watchtower");
    int32 Width = 24;
    int32 Height = 18;
    EGrid Grid = EGrid::Square;
    float UnitsPerCell = 5;
    FString ImagePath;
    float ImageScale = 1;
    float ImageX = 0;
    float ImageY = 0;
    float GridOpacity = .35f;
    TSet<FIntPoint> Walls;
    TArray<FMapLight> Lights;
    bool bVision = false;
    bool bDarkness = false;
    bool bLimitMovement = false;
    float MovementLimit = 30;
    bool bCombat = false;
    TArray<int32> TurnOrder;
    int32 TurnIndex = 0;
    int32 Round = 1;
    TArray<FMapLink> Links;
    bool Contains(FIntPoint C) const;
    FVector2D Extent() const;
    FVector2D Center(FIntPoint C) const;
    FIntPoint CellFromPoint(FVector2D P) const;
    TArray<FIntPoint> Neighbors(FIntPoint C) const;
};
struct TTRPGTABLETOP_API FLocalPlayer { int32 Id = 1; FString Name = TEXT("Mira's player"); int32 MapId = 1; };
enum class EMoveProposalState : uint8 { Pending, Approved, Rejected, Stale };
struct TTRPGTABLETOP_API FForcedMove
{
    int32 Id = 0;
    int32 MapId = 0;
    int32 ActorId = 0;
    int32 TargetId = 0;
    int32 PlayerId = 0;
    int32 TargetOwnerId = 0;
    FIntPoint ActorCell;
    FIntPoint TargetCell;
    FIntPoint Destination;
    uint64 TurnGeneration = 0;
    EMoveProposalState State = EMoveProposalState::Pending;
};

// Local preview only. A future host must resolve authenticated player identities.
class TTRPGTABLETOP_API FBoardModel
{
public:
    ERole Role = ERole::DM;
    EWorkspace Workspace = EWorkspace::MapEditor;
    bool bMapEditingLocked = true;
    ETool Tool = ETool::Select;
    int32 SelectedId = INDEX_NONE;
    int32 ActiveMapId = 1;
    int32 ViewingPlayerId = 1;
    TArray<FMapData> Maps;
    TArray<FLocalPlayer> Players;
    TArray<FToken> Tokens;
    FTokenDefaults TokenDefaults;
    TArray<FForcedMove> ForcedMoves;
    TArray<FTabletopScript> Scripts;
    TArray<FString> TriggerLog;
    TFunction<void(const FString&, int32)> OnEvent;
    TArray<FString> Activity;
    FString Status = TEXT("Choose a map or select a character.");
    uint64 Revision = 0;
    FBoardModel();
    FMapData& Map();
    const FMapData& Map() const;
    const FMapData* FindMap(int32 Id) const;
    const FLocalPlayer* Player() const;
    const FToken* Find(int32 Id) const;
    FToken* Selected();
    const FToken* Selected() const;
    void Touch();
    void Log(const FString& Message);
    bool IsInside(FIntPoint Cell) const;
    bool IsCellVisible(FIntPoint Cell) const;
    bool HasSight(const FMapData& Board, FIntPoint From, FIntPoint To) const;
    bool IsLit(const FMapData& Board, FIntPoint Cell) const;
    bool IsVisible(const FToken& Token) const;
    bool CanControl(const FToken& Token) const;
    const FToken* TokenAt(FIntPoint Cell, bool bVisibleOnly = true) const;
    bool IsAvailable(FIntPoint Cell, int32 IgnoreId = INDEX_NONE) const;
    void SetRole(ERole NewRole);
    void SetPlayer(int32 PlayerId);
    bool EnterSession(ERole Entry, int32 PlayerId = 1);
    bool HasEnteredSession() const { return bEnteredSession; }
    bool IsDMEntry() const { return !bEnteredSession || EntryRole == ERole::DM; }
    bool IsDM() const { return Role == ERole::DM && IsDMEntry(); }
    bool SetWorkspace(EWorkspace Value);
    bool CanEditMap() const;
    bool SetMapEditingLocked(bool bLocked);
    bool SwitchMap(int32 Id);
    int32 AddMap(const FString& Name, int32 Width, int32 Height, EGrid Grid);
    int32 DuplicateMap(int32 Id);
    bool DeleteMap(int32 Id);
    bool AddMapLink(const FString& Name, const FString& Url, bool bPlayerVisible);
    bool RemoveMapLink(int32 Index);
    bool ResizeMap(int32 Width, int32 Height);
    bool AssignPlayer(int32 PlayerId, int32 MapId);
    void Select(int32 Id);
    bool Move(int32 Id, FIntPoint Cell);
    bool CanRequestForcedMove(const FToken& Target) const;
    int32 RequestForcedMove(int32 TargetId, FIntPoint Cell);
    bool ResolveForcedMove(int32 ProposalId, bool bApprove);
    float PathDistance(const FToken& Token, FIntPoint Cell) const;
    float RemainingMovement(const FToken& Token) const;
    int32 ActiveTurnId() const;
    void ToggleCombat();
    void NextTurn();
    void ResetMovement();
    bool Place(ETokenKind Kind, FIntPoint Cell);
    bool PaintWall(FIntPoint Cell, bool bAdd);
    bool PaintStroke(FIntPoint From, FIntPoint To, bool bAdd);
    bool PlaceLight(FIntPoint Cell, float Range, bool bRemove = false);
    bool ToggleHidden();
    bool ChangeHP(int32 Delta);
    bool UpdateSheet(const FCharacterSheet& Sheet, const FString& Name, int32 HP);
    bool SetOwner(int32 TokenId, int32 OwnerId);
    bool CaptureTokenDefaults(int32 TokenId);
    bool ApplyTokenDefaults(int32 TokenId);
    bool GetAttribute(int32 TokenId, const FString& Attribute, float& OutValue) const;
    bool SetAttribute(int32 TokenId, const FString& Attribute, float Value);
    bool SetCondition(int32 TokenId, const FString& Condition, bool bPresent);
    bool SetStatusBars(int32 TokenId, const TArray<FStatusBar>& Bars);
    bool SetInitiative(int32 TokenId, float Value);
    int32 SaveScript(const FTabletopScript& Script);
    bool DeleteScript(int32 Id);
    bool CanReadScript(const FTabletopScript& Script) const;
    bool RunScript(int32 Id, int32 SelfId = INDEX_NONE);
    void DispatchEvent(const FString& Event, int32 TokenId);
    void EmitEvent(const FString& Event, int32 TokenId);
    void RollD20();
    FString ToJson() const;
    bool LoadJson(const FString& Json);
private:
    bool bEnteredSession = false;
    ERole EntryRole = ERole::DM;
    int32 EnteredPlayerId = 1;
    uint64 TurnGeneration = 0;
    TMap<int32, uint64> MapTurnGenerations;
    void AdvanceTurnGeneration();
    int32 NextProposalId = 1;
    bool bRunningScript = false;
    bool ExecuteScript(const FTabletopScript& Script, int32 SelfId, const FString& Event, const FString& EventCondition = FString());
    int32 NextId = 1;
    int32 NextMapId = 3;
    mutable uint64 VisionRevision = MAX_uint64;
    mutable int32 VisionMap = INDEX_NONE;
    mutable int32 VisionPlayer = INDEX_NONE;
    mutable TSet<FIntPoint> VisibleCells;
    mutable uint64 LightRevision = MAX_uint64;
    mutable int32 LightMap = INDEX_NONE;
    mutable TMap<FIntPoint, bool> LitCells;
    void RefreshVision() const;
};
}
