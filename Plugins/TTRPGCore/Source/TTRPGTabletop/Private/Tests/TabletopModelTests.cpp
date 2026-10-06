#include "TabletopModel.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
using namespace TTRPG::Tabletop;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopOwnershipTest, "TTRPG.Tabletop.PlayerPermissions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopOwnershipTest::RunTest(const FString&)
{
    FBoardModel M;
    M.SetRole(ERole::Player);
    TestTrue(TEXT("Player can move Mira"), M.Move(1, {7, 12}));
    TestFalse(TEXT("Player cannot move another hero"), M.Move(2, {8, 12}));
    TestFalse(TEXT("Player cannot place NPCs"), M.Place(ETokenKind::NPC, {7, 13}));
    TestFalse(TEXT("Player cannot paint walls"), M.PaintWall({7, 13}, true));
    TestFalse(TEXT("Player cannot hide tokens"), M.ToggleHidden());
    M.Select(2);
    TestFalse(TEXT("Player cannot edit another hero HP"), M.ChangeHP(-1));
    TestNull(TEXT("Hidden NPC absent from player hit testing"), M.TokenAt({20, 5}));
    M.Select(4);
    TestEqual(TEXT("Hidden token cannot be selected by ID"), M.SelectedId, INDEX_NONE);
    TestFalse(TEXT("Hidden occupied cell rejects movement"), M.Move(1, {20, 5}));
    TestFalse(TEXT("Hidden name not disclosed by status"), M.Status.Contains(TEXT("Lookout")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopGridTest, "TTRPG.Tabletop.GridAndPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopGridTest::RunTest(const FString&)
{
    FBoardModel M;
    TestFalse(TEXT("Negative bounds rejected"), M.Move(1, {-1, 5}));
    TestFalse(TEXT("Far bounds rejected"), M.Move(1, {24, 18}));
    TestFalse(TEXT("Walls reject movement"), M.Move(1, {5, 5}));
    TestFalse(TEXT("Occupied cells reject movement"), M.Move(1, {9, 12}));
    TestTrue(TEXT("DM can move another player's token"), M.Move(2, {9, 13}));
    TestFalse(TEXT("Cannot paint under token"), M.PaintWall({9, 13}, true));
    TestTrue(TEXT("Wall placement succeeds"), M.PaintWall({11, 10}, true));
    TestFalse(TEXT("Cannot spawn in wall"), M.Place(ETokenKind::Prop, {11, 10}));
    TestTrue(TEXT("Erasing wall opens cell"), M.PaintWall({11, 10}, false));
    TestTrue(TEXT("DM can place crate"), M.Place(ETokenKind::Prop, {11, 10}));
    TestFalse(TEXT("Cannot stack new token"), M.Place(ETokenKind::NPC, {11, 10}));
    TestTrue(TEXT("Fast wall stroke succeeds"), M.PaintStroke({12, 14}, {16, 14}, true));
    for (int32 X = 12; X <= 16; ++X) TestTrue(TEXT("Stroke fills every skipped cell"), M.Map().Walls.Contains({X, 14}));
    TestTrue(TEXT("Fast erase stroke succeeds"), M.PaintStroke({12, 14}, {16, 14}, false));
    for (int32 X = 12; X <= 16; ++X) TestFalse(TEXT("Erase clears every skipped cell"), M.Map().Walls.Contains({X, 14}));
    M.SetRole(ERole::Player);
    TestFalse(TEXT("Player cannot paint strokes"), M.PaintStroke({12, 14}, {16, 14}, true));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopPersistenceTest, "TTRPG.Tabletop.LayoutRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopPersistenceTest::RunTest(const FString&)
{
    FBoardModel Original;
    Original.Move(1, {7, 12}); Original.ChangeHP(-1);
    Original.PaintWall({11, 10}, true); Original.ToggleHidden();
    Original.Place(ETokenKind::NPC, {13, 10});
    FBoardModel Restored;
    TestTrue(TEXT("Load own serialized layout"), Restored.LoadJson(Original.ToJson()));
    TestEqual(TEXT("Token count restored"), Restored.Tokens.Num(), Original.Tokens.Num());
    TestEqual(TEXT("HP restored"), Restored.Find(1)->HP, 23);
    TestEqual(TEXT("Grid position restored"), Restored.Find(1)->Cell, FIntPoint(7, 12));
    TestTrue(TEXT("Visibility restored"), Restored.Find(1)->bHidden);
    TestTrue(TEXT("Painted wall restored"), Restored.Map().Walls.Contains({11, 10}));
    TestTrue(TEXT("Can create a token after load"), Restored.Place(ETokenKind::Hero, {13, 11}));
    TestTrue(TEXT("New ID remains unique"), Restored.Tokens.Last().Id > Original.Tokens.Last().Id);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopBadSaveTest, "TTRPG.Tabletop.InvalidSavePreservesState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopBadSaveTest::RunTest(const FString&)
{
    FBoardModel M;
    const int32 Count = M.Tokens.Num();
    TestFalse(TEXT("Corrupt save rejected"), M.LoadJson(TEXT("not json")));
    TestFalse(TEXT("Unknown version rejected"), M.LoadJson(TEXT("{\"version\":99,\"tokens\":[],\"walls\":[]}")));
    FString Duplicate = M.ToJson();
    FBoardModel Invalid;
    const FToken Copy = Invalid.Tokens[0];
    Invalid.Tokens.Add(Copy);
    TestFalse(TEXT("Duplicate IDs/cells rejected"), M.LoadJson(Invalid.ToJson()));
    TestEqual(TEXT("Invalid saves preserve tokens"), M.Tokens.Num(), Count);
    TestEqual(TEXT("Invalid saves preserve position"), M.Find(1)->Cell, FIntPoint(6, 12));
    M.SetRole(ERole::Player);
    TestFalse(TEXT("Player cannot replace map from file"), M.LoadJson(Duplicate));
    return true;
}
#endif
