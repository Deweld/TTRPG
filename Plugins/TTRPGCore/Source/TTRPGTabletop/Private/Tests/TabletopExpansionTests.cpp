#include "TabletopModel.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
using namespace TTRPG::Tabletop;
namespace
{
FBoardModel EmptyRoom()
{
    FBoardModel M;
    M.Map().Walls.Empty(); M.Map().Lights.Empty(); M.Map().Width=12; M.Map().Height=12;
    M.Tokens.SetNum(2); M.Tokens[0].Cell={2,2}; M.Tokens[1].Cell={9,9};
    return M;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapAssignmentTest,"TTRPG.Tabletop.IndependentMapsAndAssignments",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapAssignmentTest::RunTest(const FString&)
{
    FBoardModel M=EmptyRoom();
    const int32 NewId=M.AddMap(TEXT("Small hex room"),8,6,EGrid::Hex);
    TestTrue(TEXT("Independent map created"),NewId>2);
    M.PaintWall({3,3},true);
    TestEqual(TEXT("Original dimensions preserved"),M.FindMap(1)->Width,12);
    TestFalse(TEXT("Wall does not leak to another map"),M.FindMap(1)->Walls.Contains({3,3}));
    TestTrue(TEXT("Mira transfers independently"),M.AssignPlayer(1,NewId));
    TestEqual(TEXT("Mira character follows assignment"),M.Find(1)->MapId,NewId);
    TestEqual(TEXT("Rowan remains on original map"),M.Find(2)->MapId,1);
    M.SetRole(ERole::Player);
    TestEqual(TEXT("Player enters assigned map"),M.ActiveMapId,NewId);
    TestFalse(TEXT("Player cannot browse another map"),M.SwitchMap(1));
    TestFalse(TEXT("Player cannot transfer themselves"),M.AssignPlayer(1,1));
    M.SetPlayer(2);
    TestEqual(TEXT("Independent preview follows Rowan"),M.ActiveMapId,1);
    M.SetRole(ERole::DM); M.SwitchMap(NewId);
    TestTrue(TEXT("Place token near edge"),M.Place(ETokenKind::NPC,{7,5}));
    TestFalse(TEXT("Resize cannot discard tokens"),M.ResizeMap(4,4));
    int32 Full=M.AddMap(TEXT("Full"),4,4,EGrid::Square);
    for(int32 Y=0;Y<4;++Y) for(int32 X=0;X<4;++X) M.PaintWall({X,Y},true);
    TestFalse(TEXT("Full destination rejects transfer"),M.AssignPlayer(1,Full));
    TestEqual(TEXT("Failed transfer preserves source"),M.Find(1)->MapId,NewId);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridGeometryTest,"TTRPG.Tabletop.SquareHexGeometryAndPaths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGridGeometryTest::RunTest(const FString&)
{
    FBoardModel M=EmptyRoom();
    TestTrue(TEXT("Square diagonal costs sqrt(2) cells"),FMath::IsNearlyEqual(M.PathDistance(*M.Find(1),{3,3}),5.f*FMath::Sqrt(2.f),.001f));
    M.PaintWall({3,2},true); M.PaintWall({2,3},true);
    TestTrue(TEXT("No corner cutting: route must go around"),M.PathDistance(*M.Find(1),{3,3})>15);
    M.Map().Walls.Empty(); M.Map().Grid=EGrid::Hex; M.Map().UnitsPerCell=10; M.Touch();
    for(int32 Y=0;Y<12;++Y) for(int32 X=0;X<12;++X)
        TestEqual(TEXT("Hex center round trip"),M.Map().CellFromPoint(M.Map().Center({X,Y})),FIntPoint(X,Y));
    for(FIntPoint C:M.Map().Neighbors({2,2}))
        TestTrue(TEXT("Each of six hex neighbors costs one scaled cell"),FMath::IsNearlyEqual(M.PathDistance(*M.Find(1),C),10.f,.001f));
    M.Map().Grid=EGrid::Square;
    for(FIntPoint C:M.Map().Neighbors({2,2})) M.Map().Walls.Add(C);
    TestEqual(TEXT("Enclosed token has no path"),M.PathDistance(*M.Find(1),{6,6}),-1.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisionLightingTest,"TTRPG.Tabletop.DirectionalVisionWallsAndLights",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVisionLightingTest::RunTest(const FString&)
{
    FBoardModel M=EmptyRoom();
    M.Map().bVision=true; M.Tokens[0].VisionRange=20; M.Tokens[0].VisionAngle=90;
    M.SetRole(ERole::Player);
    TestTrue(TEXT("Forward cell visible"),M.IsCellVisible({4,2}));
    TestFalse(TEXT("Rear cell outside cone"),M.IsCellVisible({1,2}));
    TestFalse(TEXT("Outside range hidden"),M.IsCellVisible({7,2}));
    M.Map().Walls.Add({3,2}); M.Touch();
    TestTrue(TEXT("First wall surface visible"),M.IsCellVisible({3,2}));
    TestFalse(TEXT("Wall occludes cells behind it"),M.IsCellVisible({4,2}));
    M.Map().Walls.Empty(); M.Map().bDarkness=true; M.Touch();
    TestFalse(TEXT("Darkness requires light"),M.IsCellVisible({4,2}));
    TestTrue(TEXT("Own cell remains usable"),M.IsCellVisible({2,2}));
    M.Map().Lights.Add({{4,2},10}); M.Touch();
    TestTrue(TEXT("Light reveals a cell within vision"),M.IsCellVisible({4,2}));
    M.Map().Walls.Add({3,2}); M.Touch();
    TestFalse(TEXT("Light does not bypass a blocking wall"),M.IsCellVisible({4,2}));
    TestFalse(TEXT("Walls block illumination too"),M.IsLit(M.Map(),{2,2}));
    M.Map().Walls.Empty(); M.Map().Lights.Empty(); M.Tokens[0].bDarkvision=true; M.Touch();
    TestTrue(TEXT("Darkvision ignores absent light"),M.IsCellVisible({4,2}));
    M.Tokens[0].Facing=180; M.Touch();
    TestFalse(TEXT("Changing facing invalidates old view"),M.IsCellVisible({4,2}));
    TestTrue(TEXT("New facing reveals rear cell"),M.IsCellVisible({1,2}));
    M.SetRole(ERole::DM);
    TestTrue(TEXT("DM can always inspect the board"),M.IsCellVisible({11,11}));
    M.Map().Grid=EGrid::Hex; M.Map().Walls.Add({3,2});
    TestFalse(TEXT("Hex wall blocks a straight ray"),M.HasSight(M.Map(),{2,2},{4,2}));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementBudgetTest,"TTRPG.Tabletop.CumulativeMovementAndCombat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMovementBudgetTest::RunTest(const FString&)
{
    FBoardModel M=EmptyRoom();
    M.Map().bLimitMovement=true; M.Map().MovementLimit=10;
    M.SetRole(ERole::Player);
    TestTrue(TEXT("First step spends five units"),M.Move(1,{3,2}));
    TestTrue(TEXT("Second step exhausts budget"),M.Move(1,{4,2}));
    TestFalse(TEXT("Splitting drag does not bypass distance cap"),M.Move(1,{5,2}));
    M.ResetMovement();
    TestFalse(TEXT("Player cannot refresh their own allowance"),M.Move(1,{5,2}));
    M.SetRole(ERole::DM); M.ResetMovement();
    M.Tokens[0].Sheet.Speed=5; M.ToggleCombat();
    TestFalse(TEXT("Transfer cannot escape an active combat"),M.AssignPlayer(1,2));
    M.SetRole(ERole::Player);
    TestTrue(TEXT("Active character can spend sheet speed"),M.Move(1,{5,2}));
    TestFalse(TEXT("Sheet speed is stricter than map cap"),M.Move(1,{6,2}));
    M.SetPlayer(2);
    TestFalse(TEXT("Other player waits for turn"),M.Move(2,{8,9}));
    M.SetRole(ERole::DM); M.Tokens[1].bOutOfTurn=true;
    M.SetRole(ERole::Player);
    TestTrue(TEXT("DM exception permits movement outside turn"),M.Move(2,{8,9}));
    TestTrue(TEXT("Exception still spends allowance"),M.RemainingMovement(*M.Find(2))==5);
    M.SetRole(ERole::DM); M.NextTurn();
    TestEqual(TEXT("Next turn advances roster"),M.ActiveTurnId(),2);
    TestEqual(TEXT("New active token budget refreshed"),M.RemainingMovement(*M.Find(2)),10.f);
    TestTrue(TEXT("DM reposition ignores distance limit"),M.Move(1,{11,2}));
    M.ToggleCombat();
    TestTrue(TEXT("Transfer allowed once combat ends"),M.AssignPlayer(1,2));
    FBoardModel Loaded;
    TestTrue(TEXT("Ended combat and transfer round trip without stale turn references"),Loaded.LoadJson(M.ToJson()));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSheetPermissionsTest,"TTRPG.Tabletop.SheetAuthority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSheetPermissionsTest::RunTest(const FString&)
{
    FBoardModel M=EmptyRoom(); M.SetRole(ERole::Player);
    auto Sheet=M.Selected()->Sheet; Sheet.Notes=TEXT("Rope and lantern");
    TestTrue(TEXT("Owner edits name, HP, and notes"),M.UpdateSheet(Sheet,TEXT("Mira Vale"),12));
    Sheet.Speed=100;
    TestFalse(TEXT("Owner cannot inflate speed"),M.UpdateSheet(Sheet,TEXT("Mira Vale"),12));
    TestEqual(TEXT("Denied speed leaves original intact"),M.Selected()->Sheet.Speed,30.f);
    M.Select(2);
    TestFalse(TEXT("Another player's sheet is read-only"),M.UpdateSheet(M.Selected()->Sheet,TEXT("Changed"),1));
    M.SetRole(ERole::DM); M.Select(1);
    TestTrue(TEXT("DM can grant speed"),M.UpdateSheet(Sheet,TEXT("Mira Vale"),12));
    Sheet.Speed=std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite movement rejected"),M.UpdateSheet(Sheet,TEXT("Mira Vale"),12));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionPersistenceTest,"TTRPG.Tabletop.SessionMigrationAndValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSessionPersistenceTest::RunTest(const FString&)
{
    FBoardModel M=EmptyRoom();
    M.Map().ImagePath=TEXT("test.png"); M.Map().ImageScale=1.5; M.Map().Grid=EGrid::Hex;
    M.Map().bVision=true; M.Map().bDarkness=true; M.Map().Lights.Add({{4,4},30});
    M.Tokens[0].Sheet.Notes=TEXT("Persistent notes"); M.Tokens[0].VisionAngle=90;
    M.ToggleCombat(); M.NextTurn(); M.Tokens[1].MovementSpent=5;
    FBoardModel R;
    TestTrue(TEXT("Multi-map session loads"),R.LoadJson(M.ToJson()));
    TestEqual(TEXT("Both maps restored"),R.Maps.Num(),2);
    TestEqual(TEXT("Light restored"),R.Map().Lights.Num(),1);
    TestEqual(TEXT("Image reference restored"),R.Map().ImagePath,FString(TEXT("test.png")));
    TestEqual(TEXT("Notes restored"),R.Find(1)->Sheet.Notes,FString(TEXT("Persistent notes")));
    TestEqual(TEXT("Combat turn restored"),R.ActiveTurnId(),2);
    TestEqual(TEXT("Spent movement persists"),R.Find(2)->MovementSpent,5.f);
    M.Map().ImagePath=TEXT("../outside.png");
    TestFalse(TEXT("Image traversal rejected atomically"),R.LoadJson(M.ToJson()));
    TestEqual(TEXT("Rejected load preserved current image"),R.Map().ImagePath,FString(TEXT("test.png")));
    M.Map().ImagePath.Empty(); M.Tokens[0].MapId=2;
    TestFalse(TEXT("Owner/map mismatch rejected"),R.LoadJson(M.ToJson()));
    const FString Legacy=TEXT("{\"version\":1,\"walls\":[],\"tokens\":[{\"id\":1,\"name\":\"Mira\",\"x\":2,\"y\":3,\"kind\":0,\"hp\":17,\"owned\":true,\"hidden\":false}]}");
    TestTrue(TEXT("Existing v1 map migrates"),R.LoadJson(Legacy));
    TestEqual(TEXT("Legacy layout preserved"),R.Find(1)->Cell,FIntPoint(2,3));
    TestEqual(TEXT("Legacy owner assigned"),R.Find(1)->OwnerId,1);
    TestEqual(TEXT("Legacy HP preserved"),R.Find(1)->HP,17);
    TestTrue(TEXT("Migrated v2 session reloads"),M.LoadJson(R.ToJson()));
    return true;
}
#endif
