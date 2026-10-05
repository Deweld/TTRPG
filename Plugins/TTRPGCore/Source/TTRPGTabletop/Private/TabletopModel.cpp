#include "TabletopModel.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace TTRPG::Tabletop
{
FBoardModel::FBoardModel()
{
    for (int32 X = 1; X < Columns - 1; ++X) { Walls.Add({X, 1}); Walls.Add({X, Rows - 2}); }
    for (int32 Y = 2; Y < Rows - 2; ++Y) { Walls.Add({1, Y}); Walls.Add({Columns - 2, Y}); }
    Walls.Remove({11, Rows - 2}); Walls.Remove({12, Rows - 2});
    for (int32 X = 5; X <= 10; ++X) { if (X != 7) Walls.Add({X, 5}); }
    for (int32 Y = 6; Y <= 8; ++Y) { Walls.Add({5, Y}); Walls.Add({10, Y}); }
    for (int32 X = 14; X <= 19; ++X) { Walls.Add({X, 4}); if (X != 16 && X != 17) Walls.Add({X, 9}); }
    for (int32 Y = 5; Y <= 8; ++Y) { Walls.Add({14, Y}); Walls.Add({19, Y}); }
    Tokens.Add({NextId++, TEXT("Mira"), {6, 12}, ETokenKind::Hero, true, false, 24});
    Tokens.Add({NextId++, TEXT("Rowan"), {9, 12}, ETokenKind::Hero, false, false, 20});
    Tokens.Add({NextId++, TEXT("Sentinel"), {16, 7}, ETokenKind::NPC, false, false, 18});
    Tokens.Add({NextId++, TEXT("Lookout"), {20, 5}, ETokenKind::NPC, false, true, 12});
    Tokens.Add({NextId++, TEXT("Supply crate"), {8, 7}, ETokenKind::Prop, false, false, 1});
    Tokens.Add({NextId++, TEXT("Old chest"), {17, 12}, ETokenKind::Prop, false, false, 1});
    SelectedId = 1;
    Activity.Add(TEXT("The Watchtower is ready. Welcome to the table."));
}

bool FBoardModel::IsInside(FIntPoint Cell) const
{
    return Cell.X >= 0 && Cell.X < Columns && Cell.Y >= 0 && Cell.Y < Rows;
}
bool FBoardModel::IsVisible(const FToken& Token) const { return Role == ERole::DM || !Token.bHidden; }
bool FBoardModel::CanControl(const FToken& Token) const { return IsVisible(Token) && (Role == ERole::DM || Token.bOwnedByPlayer); }
const FToken* FBoardModel::Find(int32 Id) const { return Tokens.FindByPredicate([Id](const FToken& T) { return T.Id == Id; }); }
const FToken* FBoardModel::TokenAt(FIntPoint Cell, bool bVisibleOnly) const
{
    return Tokens.FindByPredicate([&](const FToken& T) { return T.Cell == Cell && (!bVisibleOnly || IsVisible(T)); });
}
bool FBoardModel::IsAvailable(FIntPoint Cell, int32 IgnoreId) const
{
    const FToken* Occupant = TokenAt(Cell, false);
    return IsInside(Cell) && !Walls.Contains(Cell) && (!Occupant || Occupant->Id == IgnoreId);
}
void FBoardModel::Log(const FString& Message)
{
    Status = Message;
    Activity.Insert(Message, 0);
    if (Activity.Num() > 6) Activity.SetNum(6);
}
void FBoardModel::SetRole(ERole NewRole)
{
    Role = NewRole;
    Tool = ETool::Select;
    SelectedId = 1;
    if (const FToken* Selected = Find(SelectedId); !Selected || !IsVisible(*Selected)) SelectedId = INDEX_NONE;
    // A separate player view must not expose DM-only action history.
    Activity.Empty();
    Log(Role == ERole::DM ? TEXT("DM view: all tokens and map tools available.") : TEXT("Player view: control your teal token, Mira."));
}
void FBoardModel::Select(int32 Id)
{
    const FToken* Token = Find(Id);
    if (!Token || !IsVisible(*Token)) { SelectedId = INDEX_NONE; return; }
    SelectedId = Id;
    Status = CanControl(*Token) ? TEXT("Drag the selected token, or click an empty cell to move.") : TEXT("This token belongs to another participant. View only.");
}
bool FBoardModel::Move(int32 Id, FIntPoint Cell)
{
    FToken* Token = Tokens.FindByPredicate([Id](const FToken& T) { return T.Id == Id; });
    if (!Token || !CanControl(*Token)) { Status = TEXT("You can only move a token you control."); return false; }
    if (!IsAvailable(Cell, Id)) { Status = TEXT("That cell is unavailable. Choose an open cell inside the map."); return false; }
    if (Token->Cell == Cell) return true;
    Token->Cell = Cell;
    Log(FString::Printf(TEXT("%s moved to %c%d."), *Token->Name, TCHAR('A' + Cell.X), Cell.Y + 1));
    return true;
}
bool FBoardModel::Place(ETokenKind Kind, FIntPoint Cell)
{
    if (Role != ERole::DM || !IsAvailable(Cell) || NextId >= MAX_int32) { Status = TEXT("Placement requires DM view, an open cell, and an available token ID."); return false; }
    const FString Label = Kind == ETokenKind::Hero ? TEXT("Adventurer") : Kind == ETokenKind::NPC ? TEXT("Scout") : TEXT("Crate");
    const int32 Id = NextId++;
    Tokens.Add({Id, FString::Printf(TEXT("%s %d"), *Label, Id), Cell, Kind, Kind == ETokenKind::Hero, false, Kind == ETokenKind::Prop ? 1 : 20});
    SelectedId = Id;
    Log(FString::Printf(TEXT("Placed %s at %c%d."), *Label, TCHAR('A' + Cell.X), Cell.Y + 1));
    return true;
}
bool FBoardModel::PaintWall(FIntPoint Cell, bool bAdd)
{
    if (Role != ERole::DM || !IsInside(Cell) || (bAdd && TokenAt(Cell, false))) { Status = TEXT("Walls require an empty cell and DM view."); return false; }
    if (bAdd) Walls.Add(Cell); else Walls.Remove(Cell);
    Status = bAdd ? TEXT("Wall placed. Drag to paint more cells.") : TEXT("Wall removed. Drag to clear more cells.");
    return true;
}
bool FBoardModel::PaintStroke(FIntPoint From, FIntPoint To, bool bAdd)
{
    if (Role != ERole::DM || !IsInside(From) || !IsInside(To)) return false;
    const int32 Steps = FMath::Max(FMath::Abs(To.X - From.X), FMath::Abs(To.Y - From.Y));
    bool bChanged = false;
    for (int32 I = 0; I <= Steps; ++I)
    {
        const float Alpha = Steps > 0 ? float(I) / Steps : 0.f;
        const FIntPoint Cell(FMath::RoundToInt(FMath::Lerp(float(From.X), float(To.X), Alpha)),
            FMath::RoundToInt(FMath::Lerp(float(From.Y), float(To.Y), Alpha)));
        bChanged |= PaintWall(Cell, bAdd);
    }
    return bChanged;
}
bool FBoardModel::ToggleHidden()
{
    FToken* Token = Tokens.FindByPredicate([&](const FToken& T) { return T.Id == SelectedId; });
    if (Role != ERole::DM || !Token) return false;
    Token->bHidden = !Token->bHidden;
    Log(FString::Printf(TEXT("%s is %s players."), *Token->Name, Token->bHidden ? TEXT("hidden from") : TEXT("visible to")));
    return true;
}
bool FBoardModel::ChangeHP(int32 Delta)
{
    FToken* Token = Tokens.FindByPredicate([&](const FToken& T) { return T.Id == SelectedId; });
    if (!Token || !CanControl(*Token) || Token->Kind == ETokenKind::Prop) return false;
    Token->HP = FMath::Clamp(Token->HP + FMath::Clamp(Delta, -1, 1), 0, 999);
    Log(FString::Printf(TEXT("%s now has %d HP."), *Token->Name, Token->HP));
    return true;
}
void FBoardModel::RollD20()
{
    const FToken* Token = Find(SelectedId);
    if (!Token || !CanControl(*Token) || Token->Kind == ETokenKind::Prop) { Status = TEXT("Select a character you control to roll."); return; }
    Log(FString::Printf(TEXT("%s rolled d20: %d"), *Token->Name, FMath::RandRange(1, 20)));
}
FString FBoardModel::ToJson() const
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    TArray<TSharedPtr<FJsonValue>> JsonTokens;
    for (const FToken& T : Tokens)
    {
        auto J = MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("id"), T.Id); J->SetStringField(TEXT("name"), T.Name);
        J->SetNumberField(TEXT("x"), T.Cell.X); J->SetNumberField(TEXT("y"), T.Cell.Y);
        J->SetNumberField(TEXT("kind"), int32(T.Kind)); J->SetNumberField(TEXT("hp"), T.HP);
        J->SetBoolField(TEXT("owned"), T.bOwnedByPlayer); J->SetBoolField(TEXT("hidden"), T.bHidden);
        JsonTokens.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("tokens"), JsonTokens);
    TArray<TSharedPtr<FJsonValue>> JsonWalls;
    for (const FIntPoint& W : Walls)
    {
        auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("x"), W.X); J->SetNumberField(TEXT("y"), W.Y);
        JsonWalls.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("walls"), JsonWalls);
    FString Json;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
    return Json;
}
bool FBoardModel::LoadJson(const FString& Json)
{
    auto Fail = [this]() { Status = TEXT("Could not load this map. The current layout was preserved."); return false; };
    if (Role != ERole::DM || Json.Len() > 1024 * 1024) return Fail();
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) return Fail();
    double Version;
    const TArray<TSharedPtr<FJsonValue>>* JT;
    const TArray<TSharedPtr<FJsonValue>>* JW;
    if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1
        || !Root->TryGetArrayField(TEXT("tokens"), JT) || !Root->TryGetArrayField(TEXT("walls"), JW)
        || JT->Num() > Columns * Rows || JW->Num() > Columns * Rows) return Fail();
    auto Integer = [](const FJsonObject& J, const TCHAR* Key, int32 Min, int32 Max, int32& Out)
    {
        double N;
        if (!J.TryGetNumberField(Key, N) || !FMath::IsFinite(N) || N < Min || N > Max || N != FMath::FloorToDouble(N)) return false;
        Out = int32(N); return true;
    };
    TSet<FIntPoint> NewWalls;
    TArray<FToken> NewTokens;
    TSet<int32> Ids;
    TSet<FIntPoint> Occupied;
    int32 NewNextId = 1;
    for (const auto& Value : *JW)
    {
        const TSharedPtr<FJsonObject>* J; FIntPoint C;
        if (!Value->TryGetObject(J) || !Integer(**J, TEXT("x"), 0, Columns - 1, C.X)
            || !Integer(**J, TEXT("y"), 0, Rows - 1, C.Y) || NewWalls.Contains(C)) return Fail();
        NewWalls.Add(C);
    }
    for (const auto& Value : *JT)
    {
        const TSharedPtr<FJsonObject>* J; FToken T; int32 Kind;
        if (!Value->TryGetObject(J) || !Integer(**J, TEXT("id"), 1, MAX_int32 - 1, T.Id)
            || !Integer(**J, TEXT("x"), 0, Columns - 1, T.Cell.X) || !Integer(**J, TEXT("y"), 0, Rows - 1, T.Cell.Y)
            || !Integer(**J, TEXT("kind"), 0, 2, Kind) || !Integer(**J, TEXT("hp"), 0, 999, T.HP)
            || !(*J)->TryGetStringField(TEXT("name"), T.Name) || T.Name.IsEmpty() || T.Name.Len() > 80
            || !(*J)->TryGetBoolField(TEXT("owned"), T.bOwnedByPlayer) || !(*J)->TryGetBoolField(TEXT("hidden"), T.bHidden)
            || Ids.Contains(T.Id) || Occupied.Contains(T.Cell) || NewWalls.Contains(T.Cell)) return Fail();
        T.Kind = ETokenKind(Kind); NewNextId = FMath::Max(NewNextId, T.Id + 1);
        Ids.Add(T.Id); Occupied.Add(T.Cell); NewTokens.Add(MoveTemp(T));
    }
    Tokens = MoveTemp(NewTokens); Walls = MoveTemp(NewWalls); NextId = NewNextId;
    SelectedId = INDEX_NONE; Tool = ETool::Select;
    Log(TEXT("Saved tabletop layout restored."));
    return true;
}
}
