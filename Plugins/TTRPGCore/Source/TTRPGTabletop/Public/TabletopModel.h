#pragma once

#include "CoreMinimal.h"

namespace TTRPG::Tabletop
{
enum class ERole : uint8 { DM, Player };
enum class ETool : uint8 { Select, Hero, NPC, Prop, Wall, Erase };
enum class ETokenKind : uint8 { Hero, NPC, Prop };

struct TTRPGTABLETOP_API FToken
{
    int32 Id = 0;
    FString Name;
    FIntPoint Cell;
    ETokenKind Kind = ETokenKind::Hero;
    bool bOwnedByPlayer = false;
    bool bHidden = false;
    int32 HP = 20;
};

// Local prototype state. Roles exercise UX restrictions, not network authentication.
class TTRPGTABLETOP_API FBoardModel
{
public:
    static constexpr int32 Columns = 24;
    static constexpr int32 Rows = 18;
    ERole Role = ERole::DM;
    ETool Tool = ETool::Select;
    int32 SelectedId = INDEX_NONE;
    TArray<FToken> Tokens;
    TSet<FIntPoint> Walls;
    TArray<FString> Activity;
    FString Status = TEXT("Select a token, then click a cell to move it.");

    FBoardModel();
    bool IsInside(FIntPoint Cell) const;
    bool IsVisible(const FToken& Token) const;
    bool CanControl(const FToken& Token) const;
    const FToken* Find(int32 Id) const;
    const FToken* TokenAt(FIntPoint Cell, bool bVisibleOnly = true) const;
    bool IsAvailable(FIntPoint Cell, int32 IgnoreId = INDEX_NONE) const;
    void SetRole(ERole NewRole);
    void Select(int32 Id);
    bool Move(int32 Id, FIntPoint Cell);
    bool Place(ETokenKind Kind, FIntPoint Cell);
    bool PaintWall(FIntPoint Cell, bool bAdd);
    bool PaintStroke(FIntPoint From, FIntPoint To, bool bAdd);
    bool ToggleHidden();
    bool ChangeHP(int32 Delta);
    void RollD20();
    void Log(const FString& Message);
    FString ToJson() const;
    bool LoadJson(const FString& Json);

private:
    int32 NextId = 1;
};
}
