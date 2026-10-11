#include "TabletopModel.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
using namespace TTRPG::Tabletop;
namespace
{
FBoardModel WorkspaceRoom()
{
    FBoardModel M; M.Map().Walls.Empty(); M.Map().Lights.Empty(); M.Map().Width=12; M.Map().Height=12;
    M.Tokens.SetNum(3); M.Tokens[0].Cell={2,2}; M.Tokens[1].Cell={7,2}; M.Tokens[2].Cell={5,5};
    M.Tokens[2].bHidden=false;
    return M;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkspaceAuthorityTest,"TTRPG.Tabletop.EntryAuthorityAndEditingModes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkspaceAuthorityTest::RunTest(const FString&)
{
    FBoardModel Player=WorkspaceRoom();
    TestTrue(TEXT("Player entry succeeds"),Player.EnterSession(ERole::Player,2));
    TestFalse(TEXT("Player entry cannot be re-entered as DM"),Player.EnterSession(ERole::DM));
    Player.SetRole(ERole::DM); Player.SetPlayer(1);
    TestTrue(TEXT("Player cannot elevate role"),Player.Role==ERole::Player);
    TestEqual(TEXT("Player cannot impersonate another identity"),Player.ViewingPlayerId,2);
    TestFalse(TEXT("Player cannot enter map editor"),Player.SetWorkspace(EWorkspace::MapEditor));
    TestFalse(TEXT("Player cannot unlock map editing"),Player.SetMapEditingLocked(false));
    TestFalse(TEXT("Player cannot alter layout"),Player.PaintWall({3,3},true));
    TestFalse(TEXT("Player cannot reload full DM data"),Player.LoadJson(WorkspaceRoom().ToJson()));
    Player.SetWorkspace(EWorkspace::TokenEdit);
    TestFalse(TEXT("Player sheet workspace cannot bypass gameplay movement events"),Player.Move(2,{8,2}));
    Player.SetWorkspace(EWorkspace::Gameplay);
    TestTrue(TEXT("Player gameplay movement remains allowed"),Player.Move(2,{8,2}));
    FBoardModel DM=WorkspaceRoom(); DM.EnterSession(ERole::DM);
    TestTrue(TEXT("Entry starts in gameplay"),DM.Workspace==EWorkspace::Gameplay);
    TestFalse(TEXT("Gameplay map lock blocks wall paint"),DM.PaintWall({3,3},true));
    TestFalse(TEXT("Gameplay map lock blocks map creation"),DM.AddMap(TEXT("Locked"),4,4,EGrid::Square)!=INDEX_NONE);
    TestTrue(TEXT("DM can unlock gameplay"),DM.SetMapEditingLocked(false));
    TestTrue(TEXT("Unlocked gameplay allows editing"),DM.PaintWall({3,3},true));
    DM.SetMapEditingLocked(true); DM.SetWorkspace(EWorkspace::MapEditor);
    TestTrue(TEXT("Contained editor permits layout"),DM.PaintWall({4,3},true));
    DM.SetRole(ERole::Player);
    TestFalse(TEXT("Preview does not execute DM commands"),DM.SetOwner(2,1));
    TestFalse(TEXT("Preview cannot paint walls"),DM.PaintWall({5,3},true));
    DM.SetRole(ERole::DM);
    TestTrue(TEXT("DM entry can return from player preview"),DM.IsDM());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapLifecycleTest,"TTRPG.Tabletop.MapDuplicateDeleteSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMapLifecycleTest::RunTest(const FString&)
{
    FBoardModel M=WorkspaceRoom(); M.PaintWall({8,8},true); M.Map().ImagePath=TEXT("fixture.png"); M.Map().bVision=true;
    M.ToggleCombat();
    const int32 Clone=M.DuplicateMap(1);
    TestTrue(TEXT("Map duplication produces new ID"),Clone>2);
    TestTrue(TEXT("Duplicate retains walls"),M.Map().Walls.Contains({8,8}));
    TestEqual(TEXT("Duplicate retains image reference"),M.Map().ImagePath,FString(TEXT("fixture.png")));
    TestFalse(TEXT("Duplicate starts outside combat"),M.Map().bCombat);
    TestEqual(TEXT("No combat order copied"),M.Map().TurnOrder.Num(),0);
    int32 OnClone=0; for(const FToken& T:M.Tokens) if(T.MapId==Clone)
    {++OnClone; TestEqual(TEXT("Only unowned tokens copied"),T.OwnerId,0); TestTrue(TEXT("Token instance gets new ID"),T.Id>6);}
    TestEqual(TEXT("NPC was copied once"),OnClone,1);
    TestEqual(TEXT("Player character remains on its map"),M.Find(1)->MapId,1);
    TestFalse(TEXT("Assigned combat map cannot be deleted"),M.DeleteMap(1));
    TestTrue(TEXT("Unassigned clone can be deleted"),M.DeleteMap(Clone));
    TestNull(TEXT("Deleted map removed"),M.FindMap(Clone));
    for(const FToken& T:M.Tokens) TestTrue(TEXT("No orphan tokens remain"),T.MapId!=Clone);
    TestTrue(TEXT("Unassigned cave can be deleted"),M.DeleteMap(2));
    TestFalse(TEXT("Last map cannot be deleted"),M.DeleteMap(1));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTokenDefaultsTest,"TTRPG.Tabletop.TokenDefaultsInstancesAndBars",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTokenDefaultsTest::RunTest(const FString&)
{
    FBoardModel M=WorkspaceRoom();
    TestTrue(TEXT("DM creates custom resource"),M.SetAttribute(1,TEXT("mana"),8));
    TestTrue(TEXT("DM creates resource maximum"),M.SetAttribute(1,TEXT("maxMana"),10));
    TArray<FStatusBar> Bars={FStatusBar(),{TEXT("Mana"),TEXT("mana"),TEXT("maxMana")}};
    TestTrue(TEXT("DM assigns bar bindings"),M.SetStatusBars(1,Bars));
    TestFalse(TEXT("Missing binding rejected"),M.SetStatusBars(1,{{TEXT("Missing"),TEXT("unknown"),TEXT("hp")}}));
    TestTrue(TEXT("Capture defaults"),M.CaptureTokenDefaults(1));
    TestTrue(TEXT("Instance override succeeds"),M.SetAttribute(1,TEXT("mana"),3));
    TestTrue(TEXT("Place uses captured defaults"),M.Place(ETokenKind::NPC,{9,9}));
    const int32 Created=M.SelectedId; float Value=0;
    M.GetAttribute(Created,TEXT("MANA"),Value); TestEqual(TEXT("New instance starts from default"),Value,8.f);
    M.GetAttribute(1,TEXT("mana"),Value); TestEqual(TEXT("Existing instance retains override"),Value,3.f);
    TestTrue(TEXT("Explicit apply resets this instance"),M.ApplyTokenDefaults(1));
    M.GetAttribute(1,TEXT("mana"),Value); TestEqual(TEXT("Default reapplied"),Value,8.f);
    TestEqual(TEXT("Apply preserves player ownership"),M.Find(1)->OwnerId,1);
    TestEqual(TEXT("Apply preserves position"),M.Find(1)->Cell,FIntPoint(2,2));
    TestFalse(TEXT("Ownership cannot cross map assignment"),M.SwitchMap(2) && M.SetOwner(Created,1));
    M.SwitchMap(1); TestTrue(TEXT("Ownership may be assigned on same map"),M.SetOwner(Created,2));
    M.SetRole(ERole::Player);
    TestTrue(TEXT("Owner can adjust current bar resource"),M.SetAttribute(1,TEXT("mana"),4));
    TestFalse(TEXT("Owner cannot inflate resource max"),M.SetAttribute(1,TEXT("maxmana"),999));
    TestFalse(TEXT("Owner cannot create a hidden stat"),M.SetAttribute(1,TEXT("newstat"),1));
    TestFalse(TEXT("Owner cannot grant speed"),M.SetAttribute(1,TEXT("speed"),500));
    TestFalse(TEXT("Owner cannot edit someone else"),M.SetAttribute(2,TEXT("hp"),1));
    TestFalse(TEXT("Player cannot rebind bars"),M.SetStatusBars(1,Bars));
    TestTrue(TEXT("Owner may add own condition"),M.SetCondition(1,TEXT("Poisoned"),true));
    TestTrue(TEXT("Condition names deduplicate regardless of case"),M.SetCondition(1,TEXT("poisoned"),true));
    TestEqual(TEXT("One condition stored"),M.Find(1)->Conditions.Num(),1);
    TestFalse(TEXT("Owner cannot add someone else's condition"),M.SetCondition(2,TEXT("Stunned"),true));
    TestFalse(TEXT("Nonfinite attribute rejected"),M.SetAttribute(1,TEXT("mana"),std::numeric_limits<float>::infinity()));
    TestFalse(TEXT("Invalid attribute key rejected"),M.SetAttribute(1,TEXT("mana.value"),3));
    M.SetRole(ERole::DM);
    TestFalse(TEXT("DM cannot shadow coordinates in scripts"),M.SetAttribute(1,TEXT("X"),8));
    TestFalse(TEXT("DM cannot shadow identity in scripts"),M.SetAttribute(1,TEXT("owner_id"),2));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInitiativeApprovalTest,"TTRPG.Tabletop.InitiativeAndForcedMoveApproval",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInitiativeApprovalTest::RunTest(const FString&)
{
    FBoardModel M=WorkspaceRoom(); M.SetInitiative(1,20); M.SetInitiative(2,10); M.SetInitiative(3,15); M.ToggleCombat();
    TestEqual(TEXT("Highest initiative begins"),M.ActiveTurnId(),1);
    TestEqual(TEXT("Initiative orders NPC second"),M.Map().TurnOrder[1],3);
    M.SetInitiative(2,25);
    TestEqual(TEXT("Midcombat initiative edit preserves active actor"),M.ActiveTurnId(),1);
    M.SetRole(ERole::Player);
    TestFalse(TEXT("Other token cannot be moved directly"),M.Move(2,{8,2}));
    const int32 Request=M.RequestForcedMove(2,{8,2});
    TestTrue(TEXT("Active player's proposal accepted for review"),Request!=INDEX_NONE);
    TestEqual(TEXT("Request does not move target"),M.Find(2)->Cell,FIntPoint(7,2));
    TestFalse(TEXT("Player cannot approve own request"),M.ResolveForcedMove(Request,true));
    TestEqual(TEXT("Duplicate pending request refused"),M.RequestForcedMove(2,{8,3}),INDEX_NONE);
    M.SetRole(ERole::DM);
    TestTrue(TEXT("DM approval moves target"),M.ResolveForcedMove(Request,true));
    TestEqual(TEXT("Approved destination applied"),M.Find(2)->Cell,FIntPoint(8,2));
    TestEqual(TEXT("Forced movement does not spend target budget"),M.Find(2)->MovementSpent,0.f);
    TestFalse(TEXT("Decision cannot replay"),M.ResolveForcedMove(Request,true));
    M.SetRole(ERole::Player);
    const int32 Reject=M.RequestForcedMove(2,{9,2}); M.SetRole(ERole::DM);
    TestTrue(TEXT("DM may reject a proposal"),M.ResolveForcedMove(Reject,false));
    TestEqual(TEXT("Rejection leaves target unchanged"),M.Find(2)->Cell,FIntPoint(8,2));
    M.SetRole(ERole::Player); const int32 Stale=M.RequestForcedMove(2,{9,2});
    M.SetRole(ERole::DM); M.NextTurn();
    TestFalse(TEXT("Turn change invalidates pending approval"),M.ResolveForcedMove(Stale,true));
    M.SetRole(ERole::Player);
    TestEqual(TEXT("Player cannot request outside own turn"),M.RequestForcedMove(2,{9,2}),INDEX_NONE);
    M.SetRole(ERole::DM); M.NextTurn(); M.NextTurn(); M.SetRole(ERole::Player);
    TestTrue(TEXT("Expired request does not block a later turn"),M.RequestForcedMove(2,{9,2})!=INDEX_NONE);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FApprovalRevalidationTest,"TTRPG.Tabletop.ForcedMoveRevalidatesWorldAndVisibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FApprovalRevalidationTest::RunTest(const FString&)
{
    FBoardModel M=WorkspaceRoom(); M.ToggleCombat(); M.SetRole(ERole::Player);
    int32 P=M.RequestForcedMove(2,{8,2}); M.SetRole(ERole::DM); M.PaintWall({8,2},true);
    TestFalse(TEXT("New obstacle rejects approval"),M.ResolveForcedMove(P,true));
    M.PaintWall({8,2},false); M.SetRole(ERole::Player); P=M.RequestForcedMove(2,{8,2});
    M.SetRole(ERole::DM); M.Move(2,{7,3});
    TestFalse(TEXT("Moved target rejects stale request"),M.ResolveForcedMove(P,true));
    M.Move(2,{7,2}); M.SetRole(ERole::Player); P=M.RequestForcedMove(2,{8,2});
    M.SetRole(ERole::DM); M.Map().bVision=true; M.Tokens[0].VisionRange=5; M.Touch();
    TestFalse(TEXT("Current proposer vision rechecked at approval"),M.ResolveForcedMove(P,true));
    M.SetRole(ERole::Player);
    TestEqual(TEXT("Hidden target cannot be proposed"),M.RequestForcedMove(2,{8,2}),INDEX_NONE);
    FBoardModel Independent=WorkspaceRoom(); Independent.ToggleCombat(); Independent.SetRole(ERole::Player);
    const int32 OtherMapRequest=Independent.RequestForcedMove(2,{8,2});
    Independent.SetRole(ERole::DM); Independent.SwitchMap(2); Independent.Place(ETokenKind::NPC,{0,0});
    Independent.ToggleCombat(); Independent.NextTurn();
    TestFalse(TEXT("Approval needs its map open"),Independent.ResolveForcedMove(OtherMapRequest,true));
    Independent.SwitchMap(1);
    TestTrue(TEXT("Other map turns do not invalidate this map's proposal"),Independent.ResolveForcedMove(OtherMapRequest,true));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkspaceLinksEventsTest,"TTRPG.Tabletop.MapLinksAndDomainEvents",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorkspaceLinksEventsTest::RunTest(const FString&)
{
    FBoardModel M=WorkspaceRoom();
    TestTrue(TEXT("HTTPS map link accepted"),M.AddMapLink(TEXT("Rules"),TEXT("https://example.com/rules"),true));
    TestFalse(TEXT("Local shell link rejected"),M.AddMapLink(TEXT("Bad"),TEXT("file:///C:/run.exe"),true));
    TestFalse(TEXT("Empty host rejected"),M.AddMapLink(TEXT("Bad"),TEXT("https:///bad"),true));
    TestFalse(TEXT("Whitespace link rejected"),M.AddMapLink(TEXT("Bad"),TEXT("https://example.com/ run"),true));
    TArray<FString> Events; M.OnEvent=[&Events](const FString& Name,int32 TokenId){Events.Add(Name+FString::FromInt(TokenId));};
    M.Move(1,{3,2}); M.SetCondition(1,TEXT("Blessed"),true); M.ToggleCombat();
    TestTrue(TEXT("Movement emits token event"),Events.Contains(TEXT("move1")));
    TestTrue(TEXT("Condition emits token event"),Events.Contains(TEXT("condition_added1")));
    TestTrue(TEXT("Combat emits initial turn event"),Events.Contains(TEXT("turn_start1")));
    M.SetRole(ERole::Player);
    TestFalse(TEXT("Player cannot add links"),M.AddMapLink(TEXT("Rules"),TEXT("https://example.com"),true));
    TestFalse(TEXT("Player cannot remove links"),M.RemoveMapLink(0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScopedMacroLifecycleTest,"TTRPG.Tabletop.TokenTransfersPreserveMacroScope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FScopedMacroLifecycleTest::RunTest(const FString&)
{
    FBoardModel M=WorkspaceRoom();
    FTabletopScript PlayerMacro; PlayerMacro.Id=1; PlayerMacro.Name=TEXT("Personal resource"); PlayerMacro.OwnerId=1;
    PlayerMacro.TokenId=1; PlayerMacro.MapId=1; PlayerMacro.Source=TEXT("log('personal')");
    FTabletopScript DMTrigger; DMTrigger.Id=2; DMTrigger.Name=TEXT("DM trigger"); DMTrigger.TokenId=1;
    DMTrigger.MapId=1; DMTrigger.Event=TEXT("move"); DMTrigger.Source=TEXT("log('event')");
    M.Scripts={PlayerMacro,DMTrigger};
    TestTrue(TEXT("Player can be transferred outside combat"),M.AssignPlayer(1,2));
    TestEqual(TEXT("Personal token script follows map"),M.Scripts[0].MapId,2);
    TestEqual(TEXT("DM token trigger follows map"),M.Scripts[1].MapId,2);
    FBoardModel Restored;
    TestTrue(TEXT("Transferred macro scopes remain persistable"),Restored.LoadJson(M.ToJson()));
    M.SwitchMap(2);
    TestTrue(TEXT("Owner can be cleared"),M.SetOwner(1,0));
    TestEqual(TEXT("Personal source retained"),M.Scripts[0].Source,PlayerMacro.Source);
    TestEqual(TEXT("Personal macro detached from former token"),M.Scripts[0].TokenId,0);
    TestEqual(TEXT("DM trigger retains its token binding"),M.Scripts[1].TokenId,1);
    TestTrue(TEXT("Changed ownership retains valid persistence"),Restored.LoadJson(M.ToJson()));
    return true;
}
#endif
