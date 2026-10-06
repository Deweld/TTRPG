#pragma once
#include "CoreMinimal.h"

namespace TTRPG::Tabletop
{
enum class ERole : uint8 { DM, Player };
enum class ETool : uint8 { Select, Hero, NPC, Prop, Wall, Erase, Light, EraseLight };
enum class ETokenKind : uint8 { Hero, NPC, Prop };
enum class EGrid : uint8 { Square, Hex };
struct TTRPGTABLETOP_API FCharacterSheet
{
    int32 MaxHP = 24;
    int32 Defense = 10;
    float Speed = 30;
    FString Notes;
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
};
struct TTRPGTABLETOP_API FMapLight { FIntPoint Cell; float Range = 30; };
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
    bool Contains(FIntPoint C) const;
    FVector2D Extent() const;
    FVector2D Center(FIntPoint C) const;
    FIntPoint CellFromPoint(FVector2D P) const;
    TArray<FIntPoint> Neighbors(FIntPoint C) const;
};
struct TTRPGTABLETOP_API FLocalPlayer { int32 Id = 1; FString Name = TEXT("Mira's player"); int32 MapId = 1; };

// Local preview only. A future host must resolve authenticated player identities.
class TTRPGTABLETOP_API FBoardModel
{
public:
    ERole Role = ERole::DM;
    ETool Tool = ETool::Select;
    int32 SelectedId = INDEX_NONE;
    int32 ActiveMapId = 1;
    int32 ViewingPlayerId = 1;
    TArray<FMapData> Maps;
    TArray<FLocalPlayer> Players;
    TArray<FToken> Tokens;
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
    bool SwitchMap(int32 Id);
    int32 AddMap(const FString& Name, int32 Width, int32 Height, EGrid Grid);
    bool ResizeMap(int32 Width, int32 Height);
    bool AssignPlayer(int32 PlayerId, int32 MapId);
    void Select(int32 Id);
    bool Move(int32 Id, FIntPoint Cell);
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
    void RollD20();
    FString ToJson() const;
    bool LoadJson(const FString& Json);
private:
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
