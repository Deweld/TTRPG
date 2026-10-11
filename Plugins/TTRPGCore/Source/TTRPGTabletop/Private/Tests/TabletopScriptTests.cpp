#include "TabletopScript.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
using namespace TTRPG::Tabletop;
namespace
{
FScriptContext ScriptFixture()
{
    FScriptContext Context;
    Context.PlayerId=1;Context.MapId=1;Context.SelfId=1;
    Context.MapName=TEXT("Public map");
    Context.MapAttributes={{TEXT("width"),20},{TEXT("height"),16},{TEXT("scale"),5}};
    FScriptToken Own;Own.Id=1;Own.MapId=1;Own.OwnerId=1;Own.Name=TEXT("Mira");
    Own.Attributes={{TEXT("hp"),20},{TEXT("maxHP"),24},{TEXT("speed"),30}};
    Own.WritableAttributes.Add(TEXT("hp"));Own.Conditions.Add(TEXT("Prone"));Context.Tokens.Add(Own);
    FScriptToken Other=Own;Other.Id=2;Other.OwnerId=2;Other.Name=TEXT("Rowan");Context.Tokens.Add(Other);
    FScriptToken Elsewhere=Other;Elsewhere.Id=3;Elsewhere.MapId=2;Context.Tokens.Add(Elsewhere);
    return Context;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopLuaLanguageTest,"TTRPG.Tabletop.Script.LanguageAndSnapshots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopLuaLanguageTest::RunTest(const FString&)
{
    auto Context=ScriptFixture();
    const auto Result=FTabletopScriptRuntime::Run(TEXT(
        "local t=token(self_id); assert(t.name=='Mira' and t.hp==20 and t.attrs.hp==20)\n"
        "assert(token(999)==nil and token(3)==nil and map().width==20 and role=='player')\n"
        "assert(#t.conditions==1 and t.conditions[1]=='Prone')\n"
        "local total=0; for i=1,4 do total=total+i end\n"
        "local names=0; for k,v in pairs(t.attrs) do names=names+1 end; assert(names==3)\n"
        "local a={3,1}; table.insert(a,2); table.sort(a); assert(table.concat(a,',')=='1,2,3'); table.remove(a,1)\n"
        "set_attr(self_id,'hp',t.hp-total); remove_condition(self_id,'Prone'); add_condition(self_id,'Ready')\n"
        "local dice=roll(20,2); assert(dice>=2 and dice<=40); log('Ready '..t.name)"),Context);
    TestTrue(TEXT("Ordinary Lua variables loops tables and functions execute"),Result.bSuccess);
    TestEqual(TEXT("Three proposals queued"),Result.Commands.Num(),3);
    if(Result.Commands.Num()==3)TestEqual(TEXT("Calculated HP proposal"),Result.Commands[0].Value,10.);
    TestEqual(TEXT("Log collected"),Result.Logs.Num(),1);
    TestEqual(TEXT("Input snapshot unchanged"),Context.Tokens[0].Attributes[TEXT("hp")],20.);
    TestTrue(TEXT("Globals can be local to one run"),FTabletopScriptRuntime::Run(TEXT("temporary=123"),Context).bSuccess);
    TestTrue(TEXT("State discarded between runs"),FTabletopScriptRuntime::Run(TEXT("assert(temporary==nil)"),Context).bSuccess);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopLuaPermissionsTest,"TTRPG.Tabletop.Script.PermissionsAndIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopLuaPermissionsTest::RunTest(const FString&)
{
    auto Context=ScriptFixture();
    TestTrue(TEXT("System capabilities absent"),FTabletopScriptRuntime::Run(TEXT(
        "assert(os==nil and io==nil and package==nil and require==nil and debug==nil and coroutine==nil)\n"
        "assert(load==nil and loadfile==nil and dofile==nil and pcall==nil and xpcall==nil)\n"
        "assert(collectgarbage==nil and print==nil and warn==nil and string.dump==nil and string.find==nil)\n"
        "assert(getmetatable==nil and setmetatable==nil and rawset==nil and rawget==nil)"),Context).bSuccess);
    const TArray<FString> Denied={TEXT("set_attr(2,'hp',1)"),TEXT("set_attr(1,'speed',500)"),
        TEXT("set_attr(999,'hp',1)"),TEXT("add_condition(2,'Stunned')"),TEXT("token(1).hp=999"),
        TEXT("token(1).attrs.hp=999"),TEXT("token(1).conditions[1]='Changed'"),TEXT("map().width=99"),
        TEXT("table.insert(token(1).conditions,'Changed')"),TEXT("os.execute('anything')")};
    for(const auto& Source:Denied)TestFalse(TEXT("Forbidden capability or write denied"),FTabletopScriptRuntime::Run(Source,Context).bSuccess);
    Context.bDM=true;
    TestTrue(TEXT("DM can propose mechanics on visible snapshots"),FTabletopScriptRuntime::Run(TEXT("set_attr(2,'speed',40)"),Context).bSuccess);
    TestFalse(TEXT("DM cannot access unprovided tokens"),FTabletopScriptRuntime::Run(TEXT("set_attr(999,'hp',1)"),Context).bSuccess);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTabletopLuaLimitsTest,"TTRPG.Tabletop.Script.LimitsAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTabletopLuaLimitsTest::RunTest(const FString&)
{
    const auto Context=ScriptFixture();
    const TArray<FString> Failures={
        TEXT("set_attr(1,'hp',10); log('before error'); error('stop')"),
        TEXT("set_attr(1,'hp',10); while true do end"),
        TEXT("set_attr(1,'hp',10); local huge=string.rep('x',4194304)"),
        TEXT("string.rep('',9223372036854775807)"),
        TEXT("table.move({},1,9223372036854775807,1)"),
        TEXT("table.move({},-9223372036854775807,9223372036854775807,1)"),
        TEXT("local s=string.rep('x',1024); local t={}; for i=1,100 do t[i]=s end; table.sort(t)"),
        TEXT("for i=1,65 do set_attr(1,'hp',10) end"),
        TEXT("for i=1,33 do log('message') end"),
        TEXT("set_attr(1,'hp',0/0)"),TEXT("set_attr(1,'hp',math.huge)"),
        TEXT("local function recurse() return 1+recurse() end; recurse()"),
        TEXT("this is not Lua source")};
    for(const auto& Source:Failures)
    {
        const auto Result=FTabletopScriptRuntime::Run(Source,Context);
        TestFalse(TEXT("Invalid or excessive program stopped"),Result.bSuccess);
        TestTrue(TEXT("Failure has useful diagnostic"),!Result.Error.IsEmpty());
        TestEqual(TEXT("No partial command effects"),Result.Commands.Num(),0);
        TestEqual(TEXT("No misleading successful logs"),Result.Logs.Num(),0);
    }
    const auto Big=FTabletopScriptRuntime::Run(FString::ChrN(16385,TEXT(' ')),Context);
    TestFalse(TEXT("Oversized source refused"),Big.bSuccess);
    FString Bytecode;Bytecode.AppendChar(27);Bytecode+=TEXT("Lua");
    TestFalse(TEXT("Binary chunks refused"),FTabletopScriptRuntime::Run(Bytecode,Context).bSuccess);
    const auto Memory=FTabletopScriptRuntime::Run(TEXT("local too_large=string.rep(string.rep('x',64),65536)"),Context);
    TestFalse(TEXT("Allocation cap enforced below native work limit"),Memory.bSuccess);
    TestTrue(TEXT("Allocation limit has memory diagnostic"),Memory.Error.Contains(TEXT("memory")));
    TestTrue(TEXT("Fresh run works after prior resource failures"),FTabletopScriptRuntime::Run(TEXT("assert(2+2==4)"),Context).bSuccess);
    return true;
}
#endif
