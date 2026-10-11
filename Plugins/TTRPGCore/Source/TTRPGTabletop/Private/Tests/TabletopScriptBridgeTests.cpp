#include "TabletopModel.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
using namespace TTRPG::Tabletop;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopScriptBridgeAuthorityTest,"TTRPG.Tabletop.Script.BridgeAuthorityAndAtomicity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopScriptBridgeAuthorityTest::RunTest(const FString&)
{
    FBoardModel M;M.EnterSession(ERole::DM);M.SetWorkspace(EWorkspace::Script);
    FTabletopScript S;S.Name=TEXT("Atomic test");S.MapId=1;
    S.Source=TEXT("set_attr(1,'hp',5); set_attr(1,'maxHP',0)");
    const int32 AtomicId=M.SaveScript(S);TestTrue(TEXT("DM can create macro"),AtomicId>0);
    const int32 OriginalHP=M.Find(1)->HP;
    TestFalse(TEXT("Invalid domain command rejects whole macro"),M.RunScript(AtomicId,1));
    TestEqual(TEXT("Earlier valid command not committed"),M.Find(1)->HP,OriginalHP);

    S.Id=0;S.Name=TEXT("Shared macro");S.bPlayerCallable=true;S.Source=TEXT("set_attr(2,'hp',1)");
    const int32 SharedId=M.SaveScript(S);M.SetRole(ERole::Player);
    const int32 OtherHP=M.Find(2)->HP;
    TestFalse(TEXT("Calling a DM-authored macro cannot inherit DM authority"),M.RunScript(SharedId,1));
    TestEqual(TEXT("Other player's HP unchanged"),M.Find(2)->HP,OtherHP);
    M.SetRole(ERole::DM);M.SetWorkspace(EWorkspace::Script);
    S.Id=SharedId;S.Source=TEXT("assert(token(4)==nil); assert(token(1).notes==nil); set_attr(1,'hp',19)");
    TestEqual(TEXT("DM updates macro"),M.SaveScript(S),SharedId);
    M.SetRole(ERole::Player);
    TestTrue(TEXT("Shared macro receives filtered public snapshot"),M.RunScript(SharedId,1));
    TestEqual(TEXT("Authorized HP change committed"),M.Find(1)->HP,19);
    TestFalse(TEXT("Hidden self binding rejected"),M.RunScript(SharedId,4));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopScriptBridgeEventsTest,"TTRPG.Tabletop.Script.EventScopeAndPlayerAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopScriptBridgeEventsTest::RunTest(const FString&)
{
    FBoardModel M;M.EnterSession(ERole::DM);M.SetWorkspace(EWorkspace::Script);
    FTabletopScript Trigger;Trigger.Name=TEXT("Movement trigger");Trigger.MapId=1;Trigger.TokenId=1;
    Trigger.Event=TEXT("move");Trigger.Source=TEXT("assert(role=='dm' and event=='move'); add_condition(self_id,'Ready')");
    const int32 TriggerId=M.SaveScript(Trigger);TestTrue(TEXT("DM can save token event"),TriggerId>0);
    M.SetWorkspace(EWorkspace::MapEditor);M.DispatchEvent(TEXT("move"),1);
    TestFalse(TEXT("Editor does not run automatic events"),M.Find(1)->Conditions.Contains(TEXT("Ready")));
    M.SetWorkspace(EWorkspace::Gameplay);M.DispatchEvent(TEXT("move"),2);
    TestFalse(TEXT("Binding excludes a different token"),M.Find(1)->Conditions.Contains(TEXT("Ready")));
    M.SetRole(ERole::Player);M.DispatchEvent(TEXT("move"),1);
    TestTrue(TEXT("DM local host runs event while previewing player"),M.Find(1)->Conditions.Contains(TEXT("Ready")));
    TestFalse(TEXT("Player cannot read hidden trigger"),M.CanReadScript(*M.Scripts.FindByPredicate([=](const FTabletopScript& S){return S.Id==TriggerId;})));

    FBoardModel Player;Player.EnterSession(ERole::Player,1);Player.SetWorkspace(EWorkspace::Script);
    FTabletopScript Personal;Personal.Name=TEXT("My macro");Personal.MapId=1;Personal.TokenId=1;
    Personal.Event=TEXT("move");Personal.OwnerId=0;Personal.bPlayerCallable=true;
    Personal.Source=TEXT("set_attr(self_id,'hp',18)");
    const int32 PersonalId=Player.SaveScript(Personal);TestTrue(TEXT("Player can save own macro"),PersonalId>0);
    const auto* Stored=Player.Scripts.FindByPredicate([=](const FTabletopScript& S){return S.Id==PersonalId;});
    if(Stored)
    {
        TestEqual(TEXT("Player author identity enforced"),Stored->OwnerId,1);
        TestEqual(TEXT("Player cannot install automatic trigger"),Stored->Event,FString(TEXT("manual")));
        TestFalse(TEXT("Player cannot publish DM callable macro"),Stored->bPlayerCallable);
    }
    TestTrue(TEXT("Personal macro runs on own token"),Player.RunScript(PersonalId,1));
    Personal.Id=0;Personal.TokenId=2;
    TestEqual(TEXT("Player cannot bind someone else's token"),Player.SaveScript(Personal),INDEX_NONE);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopScriptEventPayloadTest,"TTRPG.Tabletop.Script.ImmutableConditionEventPayload",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopScriptEventPayloadTest::RunTest(const FString&)
{
    FBoardModel M; M.EnterSession(ERole::DM); M.SetWorkspace(EWorkspace::Script);
    FTabletopScript First; First.Name=TEXT("Add a later condition"); First.MapId=1; First.TokenId=1;
    First.Event=TEXT("condition_added"); First.Source=TEXT("add_condition(self_id,'Other')");
    TestTrue(TEXT("First trigger saved"),M.SaveScript(First)>0);
    FTabletopScript Second=First; Second.Name=TEXT("Inspect original event");
    Second.Source=TEXT("assert(event_condition=='Poisoned'); set_attr(self_id,'hp',7)");
    TestTrue(TEXT("Second trigger saved"),M.SaveScript(Second)>0);
    M.SetWorkspace(EWorkspace::Gameplay);
    TestTrue(TEXT("Original condition added"),M.SetCondition(1,TEXT("Poisoned"),true));
    TestTrue(TEXT("Earlier trigger changed current condition list"),M.Find(1)->Conditions.Contains(TEXT("Other")));
    TestEqual(TEXT("Later trigger still saw original event payload"),M.Find(1)->HP,7);
    TestEqual(TEXT("Nested condition change did not recurse"),M.TriggerLog.Num(),2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopScriptIdentifiersTest,"TTRPG.Tabletop.Script.BoundedIDsAndUnselectedContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopScriptIdentifiersTest::RunTest(const FString&)
{
    FBoardModel Source;
    FTabletopScript High; High.Id=1000000; High.Name=TEXT("Imported upper bound"); High.MapId=1; High.Source=TEXT("log('imported')");
    Source.Scripts.Add(High);
    FBoardModel M;
    TestTrue(TEXT("Upper-bound script ID is valid saved input"),M.LoadJson(Source.ToJson()));
    M.SetWorkspace(EWorkspace::Script);
    FTabletopScript New; New.Name=TEXT("New macro after import"); New.MapId=1;
    New.Source=TEXT("assert(self_id==0); assert(token(self_id)==nil)");
    const int32 Id=M.SaveScript(New);
    TestEqual(TEXT("New script reuses first free bounded ID"),Id,1);
    TestTrue(TEXT("No selected token is represented by zero"),M.RunScript(Id));
    FBoardModel Restored;
    TestTrue(TEXT("Adding the macro keeps the session loadable"),Restored.LoadJson(M.ToJson()));
    TestEqual(TEXT("Both scripts preserved"),Restored.Scripts.Num(),2);
    return true;
}
#endif
