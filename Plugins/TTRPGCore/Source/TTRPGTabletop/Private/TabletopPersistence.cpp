#include "TabletopModel.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace TTRPG::Tabletop
{
namespace
{
using JObject = TSharedPtr<FJsonObject>;
using JValues = TArray<TSharedPtr<FJsonValue>>;
bool UnambiguousObjectKeys(const FString& Json)
{
    // The DOM may coalesce duplicate keys. Check the token stream first so a
    // repeated field or case alias cannot silently replace an earlier value.
    struct FScope { bool bObject = false; TSet<FString> Keys; };
    TArray<FScope> Scopes;
    const auto Reader = TJsonReaderFactory<>::Create(Json);
    EJsonNotation Notation;
    while (Reader->ReadNext(Notation))
    {
        if (Notation == EJsonNotation::Error) return false;
        if (Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd)
        {
            if (Scopes.IsEmpty()) return false;
            Scopes.Pop(); continue;
        }
        if (!Scopes.IsEmpty() && Scopes.Last().bObject)
        {
            const FString Key = Reader->GetIdentifier().ToLower();
            if (Scopes.Last().Keys.Contains(Key)) return false;
            Scopes.Last().Keys.Add(Key);
        }
        if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart)
        {
            if (Scopes.Num() >= 32) return false;
            FScope Scope; Scope.bObject = Notation == EJsonNotation::ObjectStart; Scopes.Add(MoveTemp(Scope));
        }
    }
    return Scopes.IsEmpty() && Reader->GetErrorMessage().IsEmpty();
}
bool Num(const FJsonObject& J, const TCHAR* Key, double Min, double Max, double& Out)
{
    return J.TryGetNumberField(Key, Out) && FMath::IsFinite(Out) && Out >= Min && Out <= Max;
}
bool Int(const FJsonObject& J, const TCHAR* Key, int32 Min, int32 Max, int32& Out)
{
    double N=0; if (!Num(J, Key, Min, Max, N) || N != FMath::FloorToDouble(N)) return false; Out = int32(N); return true;
}
bool Float(const FJsonObject& J, const TCHAR* Key, float Min, float Max, float& Out)
{
    double N=0; if (!Num(J, Key, Min, Max, N)) return false; Out = float(N); return true;
}
bool Text(const FJsonObject& J, const TCHAR* Key, FString& Out, int32 Max, bool AllowEmpty = false)
{
    return J.TryGetStringField(Key, Out) && Out.Len() <= Max && (AllowEmpty || !Out.TrimStartAndEnd().IsEmpty());
}
bool Cell(const FJsonObject& J, const FMapData& M, FIntPoint& C)
{
    return Int(J, TEXT("x"), 0, M.Width - 1, C.X) && Int(J, TEXT("y"), 0, M.Height - 1, C.Y);
}
JObject CellObject(FIntPoint C)
{
    auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("x"), C.X); J->SetNumberField(TEXT("y"), C.Y); return J;
}
bool ReadWalls(const FJsonObject& J, FMapData& M)
{
    const JValues* Values=nullptr;
    if (!J.TryGetArrayField(TEXT("walls"), Values) || Values->Num() > M.Width * M.Height) return false;
    for (const auto& V : *Values)
    {
        const JObject* O=nullptr; FIntPoint C;
        if (!V || !V->TryGetObject(O) || !Cell(**O, M, C) || M.Walls.Contains(C)) return false;
        M.Walls.Add(C);
    }
    return true;
}
bool Identifier(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 40) return false;
    for (int32 I = 0; I < Name.Len(); ++I)
    {
        const TCHAR C = Name[I];
        if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_' || (I > 0 && C >= '0' && C <= '9'))) return false;
    }
    return true;
}
bool Builtin(const FString& Name)
{
    return Name.Equals(TEXT("hp"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("maxHP"), ESearchCase::IgnoreCase)
        || Name.Equals(TEXT("defense"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("speed"), ESearchCase::IgnoreCase);
}
bool ReservedAttribute(const FString& Name)
{
    if (Builtin(Name)) return true;
    const FString Key = Name.ToLower();
    return Key == TEXT("x") || Key == TEXT("y") || Key == TEXT("id") || Key == TEXT("name")
        || Key == TEXT("map_id") || Key == TEXT("owner_id") || Key == TEXT("attrs") || Key == TEXT("conditions");
}
JObject AttributesObject(const TMap<FString, float>& Attributes)
{
    auto J = MakeShared<FJsonObject>();
    for (const auto& A : Attributes) J->SetNumberField(A.Key, A.Value);
    return J;
}
bool ReadAttributes(const FJsonObject& J, TMap<FString, float>& Attributes)
{
    const JObject* O = nullptr;
    if (!J.TryGetObjectField(TEXT("attributes"), O) || (*O)->Values.Num() > 32) return false;
    Attributes.Empty();
    for (const auto& A : (*O)->Values)
    {
        const FString Key(A.Key.Len(), *A.Key);
        double Value = 0;
        if (!Identifier(Key) || ReservedAttribute(Key) || Attributes.Contains(Key.ToLower()) || !A.Value || !A.Value->TryGetNumber(Value)
            || !FMath::IsFinite(Value) || FMath::Abs(Value) > 1000000) return false;
        Attributes.Add(Key.ToLower(), float(Value));
    }
    return true;
}
JValues BarsArray(const TArray<FStatusBar>& Bars)
{
    JValues Values;
    for (const FStatusBar& Bar : Bars)
    {
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("label"), Bar.Label);
        J->SetStringField(TEXT("value"), Bar.ValueAttribute); J->SetStringField(TEXT("maximum"), Bar.MaxAttribute);
        Values.Add(MakeShared<FJsonValueObject>(J));
    }
    return Values;
}
bool ReadBars(const FJsonObject& J, const TMap<FString, float>& Attributes, TArray<FStatusBar>& Bars)
{
    const JValues* Values = nullptr;
    if (!J.TryGetArrayField(TEXT("bars"), Values) || Values->Num() > 4) return false;
    Bars.Empty();
    auto Resolves = [&](const FString& Name) { return Identifier(Name) && (Builtin(Name) || Attributes.Contains(Name.ToLower())); };
    for (const auto& V : *Values)
    {
        const JObject* O = nullptr; FStatusBar Bar;
        if (!V || !V->TryGetObject(O) || !Text(**O, TEXT("label"), Bar.Label, 40)
            || !Text(**O, TEXT("value"), Bar.ValueAttribute, 40) || !Resolves(Bar.ValueAttribute)
            || !Text(**O, TEXT("maximum"), Bar.MaxAttribute, 40) || !Resolves(Bar.MaxAttribute)) return false;
        Bars.Add(MoveTemp(Bar));
    }
    return true;
}
void WriteSheet(FJsonObject& J, const FCharacterSheet& Sheet)
{
    J.SetNumberField(TEXT("maxHP"), Sheet.MaxHP); J.SetNumberField(TEXT("defense"), Sheet.Defense);
    J.SetNumberField(TEXT("speed"), Sheet.Speed); J.SetStringField(TEXT("notes"), Sheet.Notes);
}
bool ReadSheet(const FJsonObject& J, FCharacterSheet& Sheet)
{
    return Int(J, TEXT("maxHP"), 1, 9999, Sheet.MaxHP) && Int(J, TEXT("defense"), 0, 999, Sheet.Defense)
        && Float(J, TEXT("speed"), 0, 1000, Sheet.Speed) && Text(J, TEXT("notes"), Sheet.Notes, 4096, true);
}
bool ReadLinks(const FJsonObject& J, TArray<FMapLink>& Links)
{
    const JValues* Values = nullptr;
    if (!J.TryGetArrayField(TEXT("links"), Values) || Values->Num() > 32) return false;
    for (const auto& V : *Values)
    {
        const JObject* O = nullptr; FMapLink Link;
        if (!V || !V->TryGetObject(O) || !Text(**O, TEXT("name"), Link.Name, 80)
            || !Text(**O, TEXT("url"), Link.Url, 2048) || !(*O)->TryGetBoolField(TEXT("playerVisible"), Link.bPlayerVisible)
            || !(Link.Url.StartsWith(TEXT("https://")) || Link.Url.StartsWith(TEXT("http://")))) return false;
        for (TCHAR C : Link.Url) if (FChar::IsWhitespace(C) || C < 32 || C == '\\') return false;
        const FString Host = Link.Url.Mid(Link.Url.Find(TEXT("://")) + 3);
        if (Host.IsEmpty() || Host.StartsWith(TEXT("/")) || Host.StartsWith(TEXT("?")) || Host.StartsWith(TEXT("#"))) return false;
        Links.Add(MoveTemp(Link));
    }
    return true;
}
}
FString FBoardModel::ToJson() const
{
    auto Root = MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("version"), 3);
    Root->SetNumberField(TEXT("activeMap"), ActiveMapId); Root->SetNumberField(TEXT("viewingPlayer"), ViewingPlayerId);
    JValues JM, JT, JP;
    for (const FMapData& M : Maps)
    {
        auto J = MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("id"), M.Id); J->SetStringField(TEXT("name"), M.Name);
        J->SetNumberField(TEXT("width"), M.Width); J->SetNumberField(TEXT("height"), M.Height);
        J->SetNumberField(TEXT("grid"), int32(M.Grid)); J->SetNumberField(TEXT("scale"), M.UnitsPerCell);
        J->SetStringField(TEXT("image"), M.ImagePath); J->SetNumberField(TEXT("imageScale"), M.ImageScale);
        J->SetNumberField(TEXT("imageX"), M.ImageX); J->SetNumberField(TEXT("imageY"), M.ImageY);
        J->SetNumberField(TEXT("gridOpacity"), M.GridOpacity);
        J->SetBoolField(TEXT("vision"), M.bVision); J->SetBoolField(TEXT("darkness"), M.bDarkness);
        J->SetBoolField(TEXT("limitMovement"), M.bLimitMovement); J->SetNumberField(TEXT("movementLimit"), M.MovementLimit);
        J->SetBoolField(TEXT("combat"), M.bCombat); J->SetNumberField(TEXT("turnIndex"), M.TurnIndex); J->SetNumberField(TEXT("round"), M.Round);
        JValues Walls, Lights, Turns, Links;
        for (FIntPoint C : M.Walls) Walls.Add(MakeShared<FJsonValueObject>(CellObject(C)));
        for (const FMapLight& L : M.Lights)
        { auto O = CellObject(L.Cell); O->SetNumberField(TEXT("range"), L.Range); Lights.Add(MakeShared<FJsonValueObject>(O)); }
        for (int32 Id : M.TurnOrder) Turns.Add(MakeShared<FJsonValueNumber>(Id));
        for (const FMapLink& Link : M.Links)
        {
            auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("name"), Link.Name); O->SetStringField(TEXT("url"), Link.Url);
            O->SetBoolField(TEXT("playerVisible"), Link.bPlayerVisible); Links.Add(MakeShared<FJsonValueObject>(O));
        }
        J->SetArrayField(TEXT("walls"), Walls); J->SetArrayField(TEXT("lights"), Lights); J->SetArrayField(TEXT("turnOrder"), Turns);
        J->SetArrayField(TEXT("links"), Links);
        JM.Add(MakeShared<FJsonValueObject>(J));
    }
    for (const FLocalPlayer& P : Players)
    {
        auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("id"), P.Id); J->SetStringField(TEXT("name"), P.Name);
        J->SetNumberField(TEXT("map"), P.MapId); JP.Add(MakeShared<FJsonValueObject>(J));
    }
    for (const FToken& T : Tokens)
    {
        auto J = CellObject(T.Cell); J->SetNumberField(TEXT("id"), T.Id); J->SetStringField(TEXT("name"), T.Name);
        J->SetNumberField(TEXT("kind"), int32(T.Kind)); J->SetNumberField(TEXT("hp"), T.HP);
        J->SetNumberField(TEXT("map"), T.MapId); J->SetNumberField(TEXT("owner"), T.OwnerId); J->SetBoolField(TEXT("hidden"), T.bHidden);
        J->SetNumberField(TEXT("maxHP"), T.Sheet.MaxHP); J->SetNumberField(TEXT("defense"), T.Sheet.Defense);
        J->SetNumberField(TEXT("speed"), T.Sheet.Speed); J->SetStringField(TEXT("notes"), T.Sheet.Notes);
        J->SetNumberField(TEXT("visionRange"), T.VisionRange); J->SetNumberField(TEXT("visionAngle"), T.VisionAngle);
        J->SetNumberField(TEXT("facing"), T.Facing); J->SetBoolField(TEXT("darkvision"), T.bDarkvision);
        J->SetBoolField(TEXT("outOfTurn"), T.bOutOfTurn); J->SetNumberField(TEXT("spent"), T.MovementSpent);
        J->SetObjectField(TEXT("attributes"), AttributesObject(T.Attributes)); J->SetArrayField(TEXT("bars"), BarsArray(T.Bars));
        J->SetNumberField(TEXT("initiative"), T.Initiative);
        JValues Conditions; for (const FString& C : T.Conditions) Conditions.Add(MakeShared<FJsonValueString>(C));
        J->SetArrayField(TEXT("conditions"), Conditions);
        JT.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("maps"), JM); Root->SetArrayField(TEXT("players"), JP); Root->SetArrayField(TEXT("tokens"), JT);
    auto Defaults = MakeShared<FJsonObject>(); WriteSheet(*Defaults, TokenDefaults.Sheet);
    Defaults->SetObjectField(TEXT("attributes"), AttributesObject(TokenDefaults.Attributes)); Defaults->SetArrayField(TEXT("bars"), BarsArray(TokenDefaults.Bars));
    Defaults->SetNumberField(TEXT("visionRange"), TokenDefaults.VisionRange); Defaults->SetNumberField(TEXT("visionAngle"), TokenDefaults.VisionAngle);
    Defaults->SetBoolField(TEXT("darkvision"), TokenDefaults.bDarkvision); Root->SetObjectField(TEXT("tokenDefaults"), Defaults);
    JValues ScriptValues;
    for (const FTabletopScript& Script : Scripts)
    {
        auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("id"), Script.Id); J->SetStringField(TEXT("name"), Script.Name);
        J->SetStringField(TEXT("source"), Script.Source); J->SetNumberField(TEXT("owner"), Script.OwnerId);
        J->SetBoolField(TEXT("playerCallable"), Script.bPlayerCallable); J->SetNumberField(TEXT("map"), Script.MapId);
        J->SetNumberField(TEXT("token"), Script.TokenId); J->SetStringField(TEXT("event"), Script.Event);
        J->SetBoolField(TEXT("enabled"), Script.bEnabled); J->SetStringField(TEXT("condition"), Script.Condition);
        ScriptValues.Add(MakeShared<FJsonValueObject>(J));
    }
    Root->SetArrayField(TEXT("scripts"), ScriptValues);
    FString Json; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)); return Json;
}
bool FBoardModel::LoadJson(const FString& Json)
{
    auto Fail = [this]() { Status = TEXT("Save rejected: invalid data. Your current maps were preserved."); return false; };
    if (!IsDM() || Json.Len() > 4 * 1024 * 1024 || !UnambiguousObjectKeys(Json)) return Fail();
    JObject Root; int32 Version=0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root || !Int(*Root, TEXT("version"), 1, 3, Version)) return Fail();
    FBoardModel Candidate; Candidate.Maps.Empty(); Candidate.Tokens.Empty(); Candidate.Scripts.Empty(); Candidate.NextId = 1; Candidate.NextMapId = 1;
    const JValues* JT=nullptr;
    if (!Root->TryGetArrayField(TEXT("tokens"), JT) || JT->Num() > 512) return Fail();
    if (Version == 1)
    {
        FMapData M; if (!ReadWalls(*Root, M)) return Fail(); Candidate.Maps.Add(M); Candidate.NextMapId = 2;
    }
    else
    {
        const JValues *JM=nullptr, *JP=nullptr;
        if (!Root->TryGetArrayField(TEXT("maps"), JM) || JM->Num() < 1 || JM->Num() > 16
            || !Root->TryGetArrayField(TEXT("players"), JP) || JP->Num() < 1 || JP->Num() > 16) return Fail();
        for (const auto& Value : *JM)
        {
            const JObject* J=nullptr; FMapData M; int32 Grid=0;
            if (!Value || !Value->TryGetObject(J) || !Int(**J, TEXT("id"), 1, 1000000, M.Id) || Candidate.FindMap(M.Id)
                || !Text(**J, TEXT("name"), M.Name, 80) || !Int(**J, TEXT("width"), 4, 64, M.Width)
                || !Int(**J, TEXT("height"), 4, 64, M.Height) || !Int(**J, TEXT("grid"), 0, 1, Grid)
                || !Float(**J, TEXT("scale"), .1f, 100, M.UnitsPerCell) || !Text(**J, TEXT("image"), M.ImagePath, 200, true)
                || (!M.ImagePath.IsEmpty() && (FPaths::GetCleanFilename(M.ImagePath) != M.ImagePath || M.ImagePath.Contains(TEXT(":")) || M.ImagePath == TEXT("..")))
                || !Float(**J, TEXT("imageScale"), .1f, 5, M.ImageScale) || !Float(**J, TEXT("imageX"), -64, 64, M.ImageX)
                || !Float(**J, TEXT("imageY"), -64, 64, M.ImageY) || !Float(**J, TEXT("gridOpacity"), 0, 1, M.GridOpacity)
                || !(*J)->TryGetBoolField(TEXT("vision"), M.bVision) || !(*J)->TryGetBoolField(TEXT("darkness"), M.bDarkness)
                || !(*J)->TryGetBoolField(TEXT("limitMovement"), M.bLimitMovement) || !Float(**J, TEXT("movementLimit"), 0, 1000, M.MovementLimit)
                || !(*J)->TryGetBoolField(TEXT("combat"), M.bCombat) || !Int(**J, TEXT("turnIndex"), 0, 511, M.TurnIndex)
                || !Int(**J, TEXT("round"), 1, 1000000, M.Round) || !ReadWalls(**J, M)) return Fail();
            if (Version >= 3 && !ReadLinks(**J, M.Links)) return Fail();
            M.Grid = EGrid(Grid);
            const JValues *JL=nullptr, *JO=nullptr;
            if (!(*J)->TryGetArrayField(TEXT("lights"), JL) || JL->Num() > 32 || !(*J)->TryGetArrayField(TEXT("turnOrder"), JO) || JO->Num() > 512) return Fail();
            TSet<FIntPoint> LightCells;
            for (const auto& LV : *JL)
            {
                const JObject* L=nullptr; FMapLight Light;
                if (!LV || !LV->TryGetObject(L) || !Cell(**L, M, Light.Cell) || !Float(**L, TEXT("range"), 1, 500, Light.Range)
                    || M.Walls.Contains(Light.Cell) || LightCells.Contains(Light.Cell)) return Fail();
                LightCells.Add(Light.Cell); M.Lights.Add(Light);
            }
            TSet<int32> TurnIds;
            for (const auto& V : *JO)
            {
                double N=0; if (!V || !V->TryGetNumber(N) || !FMath::IsFinite(N) || N < 1 || N > 1000000 || N != FMath::FloorToDouble(N) || TurnIds.Contains(int32(N))) return Fail();
                TurnIds.Add(int32(N)); M.TurnOrder.Add(int32(N));
            }
            if ((M.TurnOrder.IsEmpty() && (M.bCombat || M.TurnIndex != 0)) || (!M.TurnOrder.IsEmpty() && !M.TurnOrder.IsValidIndex(M.TurnIndex))) return Fail();
            Candidate.NextMapId = FMath::Max(Candidate.NextMapId, M.Id + 1); Candidate.Maps.Add(MoveTemp(M));
        }
        Candidate.Players.Empty(); TSet<int32> PlayerIds;
        for (const auto& V : *JP)
        {
            const JObject* J=nullptr; FLocalPlayer P;
            if (!V || !V->TryGetObject(J) || !Int(**J, TEXT("id"), 1, 1000000, P.Id) || PlayerIds.Contains(P.Id)
                || !Text(**J, TEXT("name"), P.Name, 80) || !Int(**J, TEXT("map"), 1, 1000000, P.MapId) || !Candidate.FindMap(P.MapId)) return Fail();
            PlayerIds.Add(P.Id); Candidate.Players.Add(P);
        }
        if (!Int(*Root, TEXT("activeMap"), 1, 1000000, Candidate.ActiveMapId) || !Candidate.FindMap(Candidate.ActiveMapId)
            || !Int(*Root, TEXT("viewingPlayer"), 1, 1000000, Candidate.ViewingPlayerId) || !Candidate.Player()) return Fail();
    }
    TSet<int32> TokenIds; TMap<int32, TSet<FIntPoint>> Occupied;
    for (const auto& V : *JT)
    {
        const JObject* J=nullptr; FToken T; int32 Kind=0;
        if (!V || !V->TryGetObject(J) || !Int(**J, TEXT("id"), 1, 1000000, T.Id) || TokenIds.Contains(T.Id)
            || !Text(**J, TEXT("name"), T.Name, 80) || !Int(**J, TEXT("kind"), 0, 2, Kind)
            || !Int(**J, TEXT("hp"), 0, 9999, T.HP) || !(*J)->TryGetBoolField(TEXT("hidden"), T.bHidden)) return Fail();
        T.Kind = ETokenKind(Kind);
        if (Version == 1)
        {
            if (!(*J)->TryGetBoolField(TEXT("owned"), T.bOwnedByPlayer)) return Fail();
            T.OwnerId = T.bOwnedByPlayer ? 1 : T.Kind == ETokenKind::Hero ? 2 : 0;
            T.Sheet.MaxHP = FMath::Max(1, T.HP);
        }
        else if (!Int(**J, TEXT("map"), 1, 1000000, T.MapId) || !Int(**J, TEXT("owner"), 0, 1000000, T.OwnerId)
            || !Int(**J, TEXT("maxHP"), 1, 9999, T.Sheet.MaxHP) || !Int(**J, TEXT("defense"), 0, 999, T.Sheet.Defense)
            || !Float(**J, TEXT("speed"), 0, 1000, T.Sheet.Speed) || !Text(**J, TEXT("notes"), T.Sheet.Notes, 4096, true)
            || !Float(**J, TEXT("visionRange"), 0, 500, T.VisionRange) || !Float(**J, TEXT("visionAngle"), 1, 360, T.VisionAngle)
            || !Float(**J, TEXT("facing"), 0, 360, T.Facing) || !(*J)->TryGetBoolField(TEXT("darkvision"), T.bDarkvision)
            || !(*J)->TryGetBoolField(TEXT("outOfTurn"), T.bOutOfTurn) || !Float(**J, TEXT("spent"), 0, 1000, T.MovementSpent)) return Fail();
        if (Version >= 3)
        {
            const JValues* Conditions = nullptr;
            if (!ReadAttributes(**J, T.Attributes) || !ReadBars(**J, T.Attributes, T.Bars)
                || !Float(**J, TEXT("initiative"), -1000, 1000, T.Initiative)
                || !(*J)->TryGetArrayField(TEXT("conditions"), Conditions) || Conditions->Num() > 16) return Fail();
            TSet<FString> Names;
            for (const auto& C : *Conditions)
            {
                FString Name;
                if (!C || !C->TryGetString(Name) || Name.Len() > 60 || Name.TrimStartAndEnd().IsEmpty()
                    || Names.Contains(Name.TrimStartAndEnd().ToLower())) return Fail();
                Name.TrimStartAndEndInline(); Names.Add(Name.ToLower()); T.Conditions.Add(MoveTemp(Name));
            }
        }
        const FMapData* M = Candidate.FindMap(T.MapId);
        if (!M || !Cell(**J, *M, T.Cell) || M->Walls.Contains(T.Cell) || Occupied.FindOrAdd(T.MapId).Contains(T.Cell) || T.HP > T.Sheet.MaxHP) return Fail();
        if (T.OwnerId && !Candidate.Players.ContainsByPredicate([&](const FLocalPlayer& P) { return P.Id == T.OwnerId && P.MapId == T.MapId; })) return Fail();
        T.bOwnedByPlayer = T.OwnerId == 1;
        TokenIds.Add(T.Id); Occupied.FindOrAdd(T.MapId).Add(T.Cell); Candidate.NextId = FMath::Max(Candidate.NextId, T.Id + 1);
        Candidate.Tokens.Add(MoveTemp(T));
    }
    for (const FMapData& M : Candidate.Maps)
    {
        for (int32 Id : M.TurnOrder) { const FToken* T = Candidate.Find(Id); if (!T || T->MapId != M.Id || T->Kind == ETokenKind::Prop) return Fail(); }
        if (M.bCombat) for (const FToken& T : Candidate.Tokens) if (T.MapId == M.Id && T.Kind != ETokenKind::Prop && !M.TurnOrder.Contains(T.Id)) return Fail();
    }
    if (Version >= 3)
    {
        const JObject* D = nullptr; const JValues* ScriptValues = nullptr;
        FTokenDefaults& Defaults = Candidate.TokenDefaults;
        if (!Root->TryGetObjectField(TEXT("tokenDefaults"), D) || !ReadSheet(**D, Defaults.Sheet)
            || !ReadAttributes(**D, Defaults.Attributes) || !ReadBars(**D, Defaults.Attributes, Defaults.Bars)
            || !Float(**D, TEXT("visionRange"), 0, 500, Defaults.VisionRange) || !Float(**D, TEXT("visionAngle"), 1, 360, Defaults.VisionAngle)
            || !(*D)->TryGetBoolField(TEXT("darkvision"), Defaults.bDarkvision)
            || !Root->TryGetArrayField(TEXT("scripts"), ScriptValues) || ScriptValues->Num() > 64) return Fail();
        TSet<int32> ScriptIds;
        for (const auto& V : *ScriptValues)
        {
            const JObject* J = nullptr; FTabletopScript Script;
            if (!V || !V->TryGetObject(J) || !Int(**J, TEXT("id"), 1, 1000000, Script.Id) || ScriptIds.Contains(Script.Id)
                || !Text(**J, TEXT("name"), Script.Name, 80) || !Text(**J, TEXT("source"), Script.Source, 16384, true)
                || FTCHARToUTF8(*Script.Source).Length() > 16384
                || !Int(**J, TEXT("owner"), 0, 1000000, Script.OwnerId) || !(*J)->TryGetBoolField(TEXT("playerCallable"), Script.bPlayerCallable)
                || !Int(**J, TEXT("map"), 1, 1000000, Script.MapId) || !Candidate.FindMap(Script.MapId)
                || !Int(**J, TEXT("token"), 0, 1000000, Script.TokenId) || !Text(**J, TEXT("event"), Script.Event, 32)
                || !(*J)->TryGetBoolField(TEXT("enabled"), Script.bEnabled) || !Text(**J, TEXT("condition"), Script.Condition, 64, true)) return Fail();
            if (Script.Event != TEXT("manual") && Script.Event != TEXT("move") && Script.Event != TEXT("turn_start") && Script.Event != TEXT("condition_added")) return Fail();
            const FToken* ScopedToken = Script.TokenId ? Candidate.Find(Script.TokenId) : nullptr;
            if (Script.TokenId && (!ScopedToken || ScopedToken->MapId != Script.MapId)) return Fail();
            if (Script.OwnerId && (Script.Event != TEXT("manual")
                || !Candidate.Players.ContainsByPredicate([&](const FLocalPlayer& P) { return P.Id == Script.OwnerId; })
                || (ScopedToken && ScopedToken->OwnerId != Script.OwnerId))) return Fail();
            ScriptIds.Add(Script.Id); Candidate.Scripts.Add(MoveTemp(Script));
        }
    }
    Maps = MoveTemp(Candidate.Maps); Players = MoveTemp(Candidate.Players); Tokens = MoveTemp(Candidate.Tokens);
    TokenDefaults = MoveTemp(Candidate.TokenDefaults); Scripts = MoveTemp(Candidate.Scripts);
    ActiveMapId = Candidate.ActiveMapId; ViewingPlayerId = Candidate.ViewingPlayerId; NextId = Candidate.NextId; NextMapId = Candidate.NextMapId;
    // Files carry campaign state only. Entry authority, current role/workspace, and callbacks stay in this session.
    // Proposals and event logs refer to the previous live board and must never be replayed after loading.
    ForcedMoves.Empty(); MapTurnGenerations.Empty(); ++TurnGeneration; TriggerLog.Empty();
    SelectedId = INDEX_NONE; Tool = ETool::Select; Activity.Empty();
    Log(Version < 3 ? TEXT("Legacy session imported. Save to keep maps, token defaults, and scripts in format v3.")
        : TEXT("Maps, sheets, defaults, scripts, and combat state restored. Pending approvals cleared.")); return true;
}
}
