#include "TabletopModel.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
using namespace TTRPG::Tabletop;

namespace
{
TSharedPtr<FJsonObject> ReadSave(const FString& Json)
{
    TSharedPtr<FJsonObject> Root;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
    return Root;
}
FString WriteSave(const TSharedPtr<FJsonObject>& Root)
{
    FString Json; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)); return Json;
}
FTabletopScript ExampleScript()
{
    FTabletopScript Script; Script.Id = 1; Script.Name = TEXT("Arrival");
    Script.MapId = 1; Script.TokenId = 1; Script.Event = TEXT("move");
    Script.Source = TEXT("local remaining = 3");
    return Script;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopV3RoundTripTest, "TTRPG.Tabletop.PersistenceV3.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopV3RoundTripTest::RunTest(const FString&)
{
    FBoardModel M;
    M.Tokens[0].Attributes = {{TEXT("mana"), 6}, {TEXT("maxmana"), 9}};
    M.Tokens[0].Bars = {{TEXT("Health"), TEXT("hp"), TEXT("maxHP")}, {TEXT("Magic"), TEXT("mana"), TEXT("maxmana")}};
    M.Tokens[0].Conditions = {TEXT("Poisoned"), TEXT("Inspired")}; M.Tokens[0].Initiative = 17.5f;
    M.TokenDefaults.Sheet.MaxHP = 37; M.TokenDefaults.Sheet.Defense = 14;
    M.TokenDefaults.Sheet.Speed = 45; M.TokenDefaults.Sheet.Notes = TEXT("Template notes");
    M.TokenDefaults.Attributes = {{TEXT("stamina"), 8}, {TEXT("maxstamina"), 12}};
    M.TokenDefaults.Bars = {{TEXT("Stamina"), TEXT("stamina"), TEXT("maxstamina")}};
    M.TokenDefaults.VisionRange = 90; M.TokenDefaults.VisionAngle = 120; M.TokenDefaults.bDarkvision = true;
    M.Map().Links.Add({TEXT("Rules"), TEXT("https://example.com/rules"), true});
    M.Scripts.Add(ExampleScript());
    FTabletopScript PlayerMacro = ExampleScript(); PlayerMacro.Id = 2; PlayerMacro.Name = TEXT("My roll");
    PlayerMacro.Event = TEXT("manual"); PlayerMacro.OwnerId = 1; M.Scripts.Add(PlayerMacro);
    FBoardModel R;
    if (!TestTrue(TEXT("V3 save loads"), R.LoadJson(M.ToJson()))) return false;
    TestEqual(TEXT("Custom instance attribute restored"), R.Find(1)->Attributes.FindRef(TEXT("mana")), 6.f);
    TestEqual(TEXT("Both status bars restored"), R.Find(1)->Bars.Num(), 2);
    TestEqual(TEXT("Status bar binding restored"), R.Find(1)->Bars[1].MaxAttribute, FString(TEXT("maxmana")));
    TestEqual(TEXT("Condition list restored"), R.Find(1)->Conditions.Num(), 2);
    TestEqual(TEXT("Initiative restored"), R.Find(1)->Initiative, 17.5f);
    TestEqual(TEXT("Template max HP restored"), R.TokenDefaults.Sheet.MaxHP, 37);
    TestEqual(TEXT("Template speed restored"), R.TokenDefaults.Sheet.Speed, 45.f);
    TestEqual(TEXT("Template notes restored"), R.TokenDefaults.Sheet.Notes, FString(TEXT("Template notes")));
    TestEqual(TEXT("Template custom attribute restored"), R.TokenDefaults.Attributes.FindRef(TEXT("stamina")), 8.f);
    TestEqual(TEXT("Template bar restored"), R.TokenDefaults.Bars[0].Label, FString(TEXT("Stamina")));
    TestEqual(TEXT("Template directional vision restored"), R.TokenDefaults.VisionAngle, 120.f);
    TestTrue(TEXT("Template darkvision restored"), R.TokenDefaults.bDarkvision);
    TestEqual(TEXT("Map reference restored"), R.Map().Links[0].Url, FString(TEXT("https://example.com/rules")));
    TestTrue(TEXT("Reference visibility restored"), R.Map().Links[0].bPlayerVisible);
    TestEqual(TEXT("Both scripts restored"), R.Scripts.Num(), 2);
    TestEqual(TEXT("Player macro author retained"), R.Scripts[1].OwnerId, 1);
    TestEqual(TEXT("Script source restored exactly"), R.Scripts[0].Source, M.Scripts[0].Source);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopV3BoundsTest, "TTRPG.Tabletop.PersistenceV3.InvalidExtensionsAreAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopV3BoundsTest::RunTest(const FString&)
{
    auto RejectJson = [&](const TCHAR* Description, const FString& InvalidJson)
    {
        FBoardModel R; const FString Original = R.ToJson();
        TestFalse(Description, R.LoadJson(InvalidJson));
        TestTrue(TEXT("Rejected extension preserves complete saved board"), R.ToJson() == Original);
    };
    auto Reject = [&](const TCHAR* Description, const FBoardModel& Invalid) { RejectJson(Description, Invalid.ToJson()); };
    FBoardModel M;
    M.Tokens[0].Attributes.Add(TEXT("hp"), 999); Reject(TEXT("Builtin shadowing rejected"), M);
    for (const FString& Reserved : {FString(TEXT("x")), FString(TEXT("Y")), FString(TEXT("ID")), FString(TEXT("name")),
        FString(TEXT("map_id")), FString(TEXT("OWNER_ID")), FString(TEXT("attrs")), FString(TEXT("Conditions"))})
    {
        M = FBoardModel(); M.Tokens[0].Attributes.Add(Reserved, 1);
        Reject(TEXT("Token metadata cannot be shadowed by custom attributes"), M);
        M = FBoardModel(); M.TokenDefaults.Attributes.Add(Reserved, 1);
        Reject(TEXT("Token defaults cannot shadow metadata"), M);
    }
    M = FBoardModel(); M.Tokens[0].Attributes.Add(TEXT("mana"), 1);
    FString Aliases = M.ToJson(); const int32 AttributePosition = Aliases.Find(TEXT("\"mana\""));
    if (!TestTrue(TEXT("Custom attribute appears in the save fixture"), AttributePosition != INDEX_NONE)) return false;
    // Construct raw input: a TMap/JSON DOM can coalesce aliases before writing.
    Aliases.InsertAt(AttributePosition, TEXT("\"MANA\":2,"));
    RejectJson(TEXT("Case aliases cannot create duplicate attributes"), Aliases);
    FString Duplicates = M.ToJson(); Duplicates.InsertAt(AttributePosition, TEXT("\"mana\":2,"));
    RejectJson(TEXT("Repeated JSON keys rejected before DOM coalescing"), Duplicates);
    M = FBoardModel(); M.Tokens[0].Attributes.Add(TEXT("bad name"), 1); Reject(TEXT("Invalid attribute identifier rejected"), M);
    M = FBoardModel(); M.Tokens[0].Attributes.Add(TEXT("mana"), 1000001); Reject(TEXT("Attribute value limit enforced"), M);
    M = FBoardModel(); for (int32 I = 0; I < 33; ++I) M.Tokens[0].Attributes.Add(FString::Printf(TEXT("a%d"), I), 1);
    Reject(TEXT("Attribute count limit enforced"), M);
    M = FBoardModel(); M.Tokens[0].Bars = {{TEXT("Ghost"), TEXT("missing"), TEXT("maxHP")}};
    Reject(TEXT("Dangling status bar binding rejected"), M);
    M = FBoardModel(); M.Tokens[0].Bars.Init(FStatusBar(), 5); Reject(TEXT("Bar count limit enforced"), M);
    M = FBoardModel(); M.Tokens[0].Conditions = {TEXT("Poisoned"), TEXT("poisoned")};
    Reject(TEXT("Duplicate condition aliases rejected"), M);
    M = FBoardModel(); M.Tokens[0].Initiative = 1001; Reject(TEXT("Initiative limit enforced"), M);
    M = FBoardModel(); M.TokenDefaults.Sheet.Speed = 1001; Reject(TEXT("Template values validated"), M);
    M = FBoardModel(); M.Map().Links.Add({TEXT("Unsafe"), TEXT("file:///C:/private"), false});
    Reject(TEXT("Local file links rejected"), M);
    M = FBoardModel(); M.Map().Links.Add({TEXT("Unsafe"), TEXT("https://example.com/ two"), false});
    Reject(TEXT("Whitespace in links rejected"), M);
    M = FBoardModel(); M.Map().Links.Add({TEXT("Unsafe"), TEXT("https:///missing-host"), false});
    Reject(TEXT("Missing link host rejected"), M);
    M = FBoardModel(); M.Map().Links.Add({TEXT("Unsafe"), TEXT("https://example.com\\file"), false});
    Reject(TEXT("Backslash URL ambiguity rejected"), M);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopV3ScriptValidationTest, "TTRPG.Tabletop.PersistenceV3.ScriptScopesAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopV3ScriptValidationTest::RunTest(const FString&)
{
    FBoardModel R; const FString Original = R.ToJson();
    auto Reject = [&](const TCHAR* Description, const FTabletopScript& Script)
    {
        FBoardModel Invalid; Invalid.Scripts.Add(Script);
        TestFalse(Description, R.LoadJson(Invalid.ToJson()));
        TestEqual(TEXT("Invalid script preserves complete saved board"), R.ToJson(), Original);
    };
    FTabletopScript S = ExampleScript(); S.MapId = 999; Reject(TEXT("Missing map reference rejected"), S);
    S = ExampleScript(); S.MapId = 2; Reject(TEXT("Cross-map token scope rejected"), S);
    S = ExampleScript(); S.TokenId = 999; Reject(TEXT("Missing token scope rejected"), S);
    S = ExampleScript(); S.Event = TEXT("shell"); Reject(TEXT("Unknown event rejected"), S);
    S = ExampleScript(); S.Source = FString::ChrN(16385, 'a'); Reject(TEXT("Source size cap enforced"), S);
    S = ExampleScript(); S.Source = FString::ChrN(9000, TCHAR(0x03BB)); Reject(TEXT("UTF-8 source size cap enforced"), S);
    S = ExampleScript(); S.Condition = FString::ChrN(65, 'a'); Reject(TEXT("Script prerequisite size cap enforced"), S);
    S = ExampleScript(); S.OwnerId = 1; Reject(TEXT("Player-authored automatic triggers rejected"), S);
    S = ExampleScript(); S.OwnerId = 2; S.Event = TEXT("manual"); Reject(TEXT("Player cannot own another token's macro"), S);
    S = ExampleScript(); S.OwnerId = 999; S.Event = TEXT("manual"); Reject(TEXT("Unknown macro author rejected"), S);
    FBoardModel Invalid; Invalid.Scripts = {ExampleScript(), ExampleScript()};
    TestFalse(TEXT("Duplicate script IDs rejected"), R.LoadJson(Invalid.ToJson()));
    Invalid.Scripts.Empty();
    for (int32 I = 1; I <= 65; ++I) { S = ExampleScript(); S.Id = I; Invalid.Scripts.Add(S); }
    TestFalse(TEXT("Script count cap enforced"), R.LoadJson(Invalid.ToJson()));
    FBoardModel Valid; S = ExampleScript(); S.OwnerId = 1; S.Event = TEXT("manual"); S.TokenId = 0;
    Valid.Scripts.Add(S); Valid.Players[0].MapId = 2; Valid.Tokens[0].MapId = 2; Valid.Tokens[0].Cell = {0, 0};
    TestTrue(TEXT("Old-map player macro survives transfer and remains scoped to its map"), R.LoadJson(Valid.ToJson()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopV3AuthorityTest, "TTRPG.Tabletop.PersistenceV3.SessionAuthorityAndTransientState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopV3AuthorityTest::RunTest(const FString&)
{
    FBoardModel Source;
    auto Root = ReadSave(Source.ToJson());
    Root->SetStringField(TEXT("role"), TEXT("DM")); Root->SetStringField(TEXT("entryRole"), TEXT("DM"));
    Root->SetBoolField(TEXT("enteredSession"), false); Root->SetNumberField(TEXT("workspace"), int32(EWorkspace::MapEditor));
    Root->SetBoolField(TEXT("mapEditingLocked"), false);
    const FString Forged = WriteSave(Root);
    FBoardModel Player;
    TestTrue(TEXT("Player entry selected"), Player.EnterSession(ERole::Player, 1));
    TestFalse(TEXT("Saved authority fields cannot grant DM load permission"), Player.LoadJson(Forged));
    TestFalse(TEXT("Failed load cannot reset one-way entry selection"), Player.EnterSession(ERole::DM));
    TestFalse(TEXT("Player remains without DM authority"), Player.IsDMEntry());
    FBoardModel DM; TestTrue(TEXT("DM entry selected"), DM.EnterSession(ERole::DM));
    DM.SetWorkspace(EWorkspace::Gameplay); DM.SetMapEditingLocked(true);
    FForcedMove Pending; Pending.Id = 77; DM.ForcedMoves.Add(Pending); DM.TriggerLog.Add(TEXT("Old event"));
    int32 EventCount = 0; DM.OnEvent = [&](const FString&, int32) { ++EventCount; };
    TestTrue(TEXT("DM can load state with unknown authority fields ignored"), DM.LoadJson(Forged));
    TestTrue(TEXT("Entry decision retained"), DM.HasEnteredSession() && DM.IsDMEntry());
    TestEqual(TEXT("Current workspace retained"), int32(DM.Workspace), int32(EWorkspace::Gameplay));
    TestTrue(TEXT("Current edit lock retained"), DM.bMapEditingLocked);
    TestTrue(TEXT("Old live proposals cleared"), DM.ForcedMoves.IsEmpty());
    TestTrue(TEXT("Old event log cleared"), DM.TriggerLog.IsEmpty());
    TestEqual(TEXT("Load emits no game events"), EventCount, 0);
    TestTrue(TEXT("Existing callback preserved"), bool(DM.OnEvent));
    const auto Saved = ReadSave(DM.ToJson());
    TestFalse(TEXT("Role is not serialized"), Saved->HasField(TEXT("role")));
    TestFalse(TEXT("Entry authority is not serialized"), Saved->HasField(TEXT("entryRole")));
    TestFalse(TEXT("Transient proposals are not serialized"), Saved->HasField(TEXT("forcedMoves")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopV3MigrationTest, "TTRPG.Tabletop.PersistenceV3.V2MigrationAndNoScriptExecution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopV3MigrationTest::RunTest(const FString&)
{
    FBoardModel Source; auto V2 = ReadSave(Source.ToJson()); V2->SetNumberField(TEXT("version"), 2);
    V2->RemoveField(TEXT("tokenDefaults")); V2->RemoveField(TEXT("scripts"));
    for (const auto& V : V2->GetArrayField(TEXT("maps"))) V->AsObject()->RemoveField(TEXT("links"));
    for (const auto& V : V2->GetArrayField(TEXT("tokens")))
    {
        const auto& T = V->AsObject(); T->RemoveField(TEXT("attributes")); T->RemoveField(TEXT("bars"));
        T->RemoveField(TEXT("conditions")); T->RemoveField(TEXT("initiative"));
    }
    FBoardModel R;
    TestTrue(TEXT("Original v2 schema loads"), R.LoadJson(WriteSave(V2)));
    TestEqual(TEXT("V2 token position retained"), R.Find(1)->Cell, Source.Find(1)->Cell);
    TestEqual(TEXT("V2 receives default HP bar"), R.Find(1)->Bars.Num(), 1);
    TestEqual(TEXT("V2 receives usable creation defaults"), R.TokenDefaults.Sheet.MaxHP, 24);
    TestTrue(TEXT("V2 migration invents no scripts"), R.Scripts.IsEmpty());
    FTabletopScript Draft = ExampleScript(); Draft.Source = TEXT("unfinished script ???"); Source.Scripts.Add(Draft);
    int32 EventCount = 0; R.OnEvent = [&](const FString&, int32) { ++EventCount; };
    TestTrue(TEXT("Loading stores script source without running or compiling editor drafts"), R.LoadJson(Source.ToJson()));
    TestEqual(TEXT("Load does not execute token/map event callbacks"), EventCount, 0);
    TestEqual(TEXT("Script source preserved for later editing"), R.Scripts[0].Source, Draft.Source);
    TestEqual(TEXT("Load does not mutate HP through scripts"), R.Find(1)->HP, Source.Find(1)->HP);
    return true;
}
#endif
