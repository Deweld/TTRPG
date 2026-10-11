#include "TabletopScript.h"
#include "HAL/PlatformTime.h"
#include <cstdlib>
#include <cstring>
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace TTRPG::Tabletop
{
namespace
{
constexpr int32 HookInterval = 1000;
struct FRunState
{
    const FScriptContext* Context = nullptr;
    FScriptResult Result;
    FRandomStream Random;
    SIZE_T Allocated = 0;
    int32 Instructions = 0;
    double Deadline = 0;
    int32 TokenRegistry = LUA_NOREF;
    int32 MapRegistry = LUA_NOREF;
    TArray<ANSICHAR> Scratch;
    TArray<ANSICHAR> Source;
};

void* BoundedAllocate(void* User, void* Pointer, size_t OldSize, size_t NewSize)
{
    auto* State = static_cast<FRunState*>(User);
    const SIZE_T Existing = Pointer ? OldSize : 0;
    if (!NewSize)
    {
        std::free(Pointer);
        State->Allocated -= Existing;
        return nullptr;
    }
    if (NewSize > FTabletopScriptRuntime::MaxMemory - (State->Allocated - Existing)) return nullptr;
    void* Result = std::realloc(Pointer, NewSize);
    if (Result) State->Allocated = State->Allocated - Existing + NewSize;
    return Result;
}
FRunState& State(lua_State* L)
{
    void* User = nullptr;
    lua_getallocf(L, &User);
    return *static_cast<FRunState*>(User);
}
// Keep conversion/allocation helpers uninlined: on Win64, longjmp performs
// structured unwinding. Even a completed FString temporary can introduce an
// incompatible C++ EH frame into a Lua callback built with Unreal's EH settings.
// Helpers never call Lua and have returned before any Lua error can occur.
FORCENOINLINE void CopyUtf8(const FString& Value, TArray<ANSICHAR>& Output)
{
    const FTCHARToUTF8 Converted(*Value);
    Output.SetNumUninitialized(Converted.Length() + 1);
    FMemory::Memcpy(Output.GetData(), Converted.Get(), Converted.Length() + 1);
}
// Lua's C error handling uses longjmp. Conversion temporaries are destroyed
// before making a Lua call that can raise, and callback output lives in State.
void PushText(lua_State* L, const FString& Value)
{
    CopyUtf8(Value, State(L).Scratch);
    lua_pushlstring(L, State(L).Scratch.GetData(), State(L).Scratch.Num() - 1);
}
void SetNumber(lua_State* L, const char* Key, double Value)
{
    lua_pushnumber(L, Value);
    lua_setfield(L, -2, Key);
}
void SetText(lua_State* L, const char* Key, const FString& Value)
{
    PushText(L, Value);
    lua_setfield(L, -2, Key);
}
void PutAttributes(lua_State* L, const TMap<FString, double>& Attributes)
{
    for (const auto& Attribute : Attributes)
    {
        PushText(L, Attribute.Key);
        lua_pushnumber(L, Attribute.Value);
        lua_rawset(L, -3);
    }
}
int ReadOnlyWrite(lua_State* L) { return luaL_error(L, "snapshots are read-only; use queued commands"); }
int ReadOnlyNext(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, 2);
    if (!lua_next(L, -2)) return 0;
    lua_remove(L, -3);
    return 2;
}
int ReadOnlyPairs(lua_State* L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushcclosure(L, ReadOnlyNext, 1);
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
}
int ReadOnlyLength(lua_State* L)
{
    lua_pushinteger(L, lua_rawlen(L, lua_upvalueindex(1)));
    return 1;
}
// Replaces the backing table on top with an empty immutable proxy. The backing
// table is reachable only through protected metamethods, never iterator state.
void WrapReadOnly(lua_State* L)
{
    const int Backing = lua_absindex(L, -1);
    lua_newtable(L);
    lua_newtable(L);
    lua_pushvalue(L, Backing);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, ReadOnlyWrite);
    lua_setfield(L, -2, "__newindex");
    lua_pushvalue(L, Backing);
    lua_pushcclosure(L, ReadOnlyPairs, 1);
    lua_setfield(L, -2, "__pairs");
    lua_pushvalue(L, Backing);
    lua_pushcclosure(L, ReadOnlyLength, 1);
    lua_setfield(L, -2, "__len");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);
    lua_remove(L, Backing);
}
void InstructionHook(lua_State* L, lua_Debug*)
{
    auto& Run = State(L);
    Run.Instructions += HookInterval;
    if (Run.Instructions >= FTabletopScriptRuntime::MaxInstructions || FPlatformTime::Seconds() > Run.Deadline)
        luaL_error(L, "script execution limit exceeded");
}
int GetToken(lua_State* L)
{
    const lua_Integer Id = luaL_checkinteger(L, 1);
    lua_rawgeti(L, LUA_REGISTRYINDEX, State(L).TokenRegistry);
    lua_rawgeti(L, -1, Id);
    return 1;
}
int GetMap(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, State(L).MapRegistry);
    return 1;
}
const FScriptToken* WritableToken(const FRunState& Run, lua_Integer Id)
{
    for (const auto& Token : Run.Context->Tokens)
        if (Token.Id == Id && Token.MapId == Run.Context->MapId &&
            (Run.Context->bDM || (Run.Context->PlayerId > 0 && Token.OwnerId == Run.Context->PlayerId))) return &Token;
    return nullptr;
}
const char* CheckName(lua_State* L, int Index)
{
    size_t Length = 0;
    const char* Value = luaL_checklstring(L, Index, &Length);
    if (!Length || Length > 256 || std::memchr(Value, 0, Length)) luaL_error(L, "name must be 1-64 characters without nulls");
    return Value;
}
FORCENOINLINE bool CanSet(const FScriptToken& Token, const FRunState& Run, const char* Name)
{
    const FString Key = UTF8_TO_TCHAR(Name);
    return Key.Len() <= 64 && (Run.Context->bDM || Token.WritableAttributes.Contains(Key));
}
FORCENOINLINE void QueueAttribute(FRunState& Run, int32 Id, const char* Name, double Value)
{
    FScriptCommand& Command = Run.Result.Commands.AddDefaulted_GetRef();
    Command.Kind = EScriptCommand::SetAttribute;
    Command.TargetId = Id;
    Command.Name = UTF8_TO_TCHAR(Name);
    Command.Value = Value;
}
int SetAttribute(lua_State* L)
{
    const lua_Integer Id = luaL_checkinteger(L, 1);
    const char* Name = CheckName(L, 2);
    const double Value = luaL_checknumber(L, 3);
    auto& Run = State(L);
    const FScriptToken* Token = WritableToken(Run, Id);
    if (!Token || !CanSet(*Token, Run, Name)) return luaL_error(L, "attribute write is not authorized");
    if (!FMath::IsFinite(Value) || FMath::Abs(Value) > 1000000) return luaL_error(L, "attribute value out of range");
    if (Run.Result.Commands.Num() >= FTabletopScriptRuntime::MaxCommands) return luaL_error(L, "too many script commands");
    QueueAttribute(Run, static_cast<int32>(Id), Name, Value);
    return 0;
}
FORCENOINLINE bool ValidCondition(const char* Name)
{
    const FString Value = UTF8_TO_TCHAR(Name);
    return Value.Len() <= 60 && !Value.TrimStartAndEnd().IsEmpty();
}
FORCENOINLINE void QueueCondition(FRunState& Run, int32 Id, const char* Name, EScriptCommand Kind)
{
    FScriptCommand& Command = Run.Result.Commands.AddDefaulted_GetRef();
    Command.Kind = Kind;
    Command.TargetId = Id;
    Command.Name = UTF8_TO_TCHAR(Name);
}
int ChangeCondition(lua_State* L, EScriptCommand Kind)
{
    const lua_Integer Id = luaL_checkinteger(L, 1);
    const char* Name = CheckName(L, 2);
    auto& Run = State(L);
    if (!WritableToken(Run, Id)) return luaL_error(L, "condition write is not authorized");
    if (!ValidCondition(Name)) return luaL_error(L, "invalid condition name");
    if (Run.Result.Commands.Num() >= FTabletopScriptRuntime::MaxCommands) return luaL_error(L, "too many script commands");
    QueueCondition(Run, static_cast<int32>(Id), Name, Kind);
    return 0;
}
int AddCondition(lua_State* L) { return ChangeCondition(L, EScriptCommand::AddCondition); }
int RemoveCondition(lua_State* L) { return ChangeCondition(L, EScriptCommand::RemoveCondition); }
FORCENOINLINE void AppendLog(FRunState& Run, const char* Message)
{
    Run.Result.Logs.Add(FString(UTF8_TO_TCHAR(Message)).Left(256));
}
int Log(lua_State* L)
{
    size_t Length = 0;
    const char* Message = luaL_checklstring(L, 1, &Length);
    auto& Run = State(L);
    if (Length > 1024 || std::memchr(Message, 0, Length)) return luaL_error(L, "log message too long or contains nulls");
    if (Run.Result.Logs.Num() >= FTabletopScriptRuntime::MaxLogs) return luaL_error(L, "too many log messages");
    AppendLog(Run, Message);
    return 0;
}
int Roll(lua_State* L)
{
    const lua_Integer Sides = luaL_checkinteger(L, 1);
    const lua_Integer Count = luaL_optinteger(L, 2, 1);
    if (Sides < 2 || Sides > 1000 || Count < 1 || Count > 100) return luaL_error(L, "roll expects 2-1000 sides and 1-100 dice");
    int32 Total = 0;
    for (int32 Index = 0; Index < Count; ++Index) Total += State(L).Random.RandRange(1, static_cast<int32>(Sides));
    lua_pushinteger(L, Total);
    return 1;
}
void CheckNativeBudget(lua_State* L, int32 Work)
{
    auto& Run = State(L);
    Run.Instructions += Work;
    if (Run.Instructions >= FTabletopScriptRuntime::MaxInstructions || FPlatformTime::Seconds() > Run.Deadline)
        luaL_error(L, "script execution limit exceeded");
}
int BoundedRepeat(lua_State* L)
{
    const lua_Integer Count = luaL_checkinteger(L, 2);
    // Empty strings otherwise allow arbitrarily long native loops with no
    // allocation and no VM instruction hook.
    if (Count > 65536) return luaL_error(L, "string.rep count exceeds 65536");
    CheckNativeBudget(L, Count > 0 ? static_cast<int32>(Count) : 1);
    const int Arguments = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1)); lua_insert(L, 1);
    lua_call(L, Arguments, LUA_MULTRET);
    CheckNativeBudget(L, 0);
    return lua_gettop(L);
}
enum class ETableGuard : int { Length, Move, Concat, Unpack, Sort };
int BoundedTable(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto Guard = static_cast<ETableGuard>(lua_tointeger(L, lua_upvalueindex(2)));
    lua_Integer Count = 0;
    if (Guard == ETableGuard::Move || Guard == ETableGuard::Concat || Guard == ETableGuard::Unpack)
    {
        const int StartIndex = Guard == ETableGuard::Concat ? 3 : 2;
        const lua_Integer From = Guard == ETableGuard::Move ? luaL_checkinteger(L, 2) : luaL_optinteger(L, StartIndex, 1);
        const lua_Integer To = Guard == ETableGuard::Move ? luaL_checkinteger(L, 3) : luaL_optinteger(L, StartIndex + 1, luaL_len(L, 1));
        if (To >= From)
        {
            // Unsigned subtraction also handles signed ranges crossing zero.
            const lua_Unsigned Difference = static_cast<lua_Unsigned>(To) - static_cast<lua_Unsigned>(From);
            if (Difference >= 4096) return luaL_error(L, "table operation range exceeds 4096");
            Count = static_cast<lua_Integer>(Difference + 1);
        }
    }
    else
    {
        Count = luaL_len(L, 1);
        if (Count > (Guard == ETableGuard::Sort ? 256 : 4096)) return luaL_error(L, "table operation size exceeds limit");
    }
    if (Guard == ETableGuard::Sort)
    {
        size_t TotalBytes = 0;
        for (lua_Integer Index = 1; Index <= Count; ++Index)
        {
            lua_geti(L, 1, Index);
            if (lua_type(L, -1) == LUA_TSTRING) TotalBytes += lua_rawlen(L, -1);
            lua_pop(L, 1);
            // Repeated references to one long string must still count each
            // comparison's input; memory limits alone cannot bound sort work.
            if (TotalBytes > 65536) return luaL_error(L, "table.sort string input exceeds 64 KiB");
        }
    }
    CheckNativeBudget(L, static_cast<int32>(Count) + 1);
    const int Arguments = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1)); lua_insert(L, 1);
    lua_call(L, Arguments, LUA_MULTRET);
    CheckNativeBudget(L, 0);
    return lua_gettop(L);
}
void GuardTableFunction(lua_State* L, const char* Name, ETableGuard Guard)
{
    lua_getglobal(L, LUA_TABLIBNAME);
    lua_getfield(L, -1, Name);
    lua_pushinteger(L, static_cast<int>(Guard));
    lua_pushcclosure(L, BoundedTable, 2);
    lua_setfield(L, -2, Name);
    lua_pop(L, 1);
}
void RemoveGlobals(lua_State* L, const char* const* Names)
{
    for (int32 Index = 0; Names[Index]; ++Index) { lua_pushnil(L); lua_setglobal(L, Names[Index]); }
}
void RemoveFields(lua_State* L, const char* Table, const char* const* Names)
{
    lua_getglobal(L, Table);
    for (int32 Index = 0; Names[Index]; ++Index) { lua_pushnil(L); lua_setfield(L, -2, Names[Index]); }
    lua_pop(L, 1);
}
int Execute(lua_State* L)
{
    auto& Run = State(L);
    const auto& Context = *Run.Context;
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1); lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1); lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1); lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1); lua_pop(L, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1); lua_pop(L, 1);
    // Protected calls and coroutines could catch/yield around termination hooks.
    const char* const Globals[] = { "load", "loadfile", "dofile", "collectgarbage", "print", "warn", "pcall", "xpcall", "getmetatable", "setmetatable", "rawset", "rawget", "_G", nullptr };
    RemoveGlobals(L, Globals);
    // Pattern matching executes unmetered native loops; dump exposes bytecode.
    const char* const StringFields[] = { "dump", "find", "match", "gmatch", "gsub", nullptr };
    RemoveFields(L, LUA_STRLIBNAME, StringFields);
    const char* const MathFields[] = { "random", "randomseed", nullptr };
    RemoveFields(L, LUA_MATHLIBNAME, MathFields);
    lua_getglobal(L, LUA_STRLIBNAME); lua_getfield(L, -1, "rep");
    lua_pushcclosure(L, BoundedRepeat, 1); lua_setfield(L, -2, "rep"); lua_pop(L, 1);
    GuardTableFunction(L, "insert", ETableGuard::Length);
    GuardTableFunction(L, "remove", ETableGuard::Length);
    GuardTableFunction(L, "move", ETableGuard::Move);
    GuardTableFunction(L, "concat", ETableGuard::Concat);
    GuardTableFunction(L, "unpack", ETableGuard::Unpack);
    GuardTableFunction(L, "sort", ETableGuard::Sort);

    lua_newtable(L);
    for (const auto& Token : Context.Tokens)
    {
        if (Token.MapId != Context.MapId) continue;
        lua_newtable(L);
        PutAttributes(L, Token.Attributes);
        SetNumber(L, "id", Token.Id); SetNumber(L, "map_id", Token.MapId); SetNumber(L, "owner_id", Token.OwnerId);
        SetText(L, "name", Token.Name);
        lua_newtable(L); PutAttributes(L, Token.Attributes); WrapReadOnly(L); lua_setfield(L, -2, "attrs");
        lua_newtable(L);
        for (int32 Index = 0; Index < Token.Conditions.Num(); ++Index)
        {
            PushText(L, Token.Conditions[Index]); lua_rawseti(L, -2, Index + 1);
        }
        WrapReadOnly(L); lua_setfield(L, -2, "conditions");
        WrapReadOnly(L); lua_rawseti(L, -2, Token.Id);
    }
    Run.TokenRegistry = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L); PutAttributes(L, Context.MapAttributes);
    SetNumber(L, "id", Context.MapId); SetText(L, "name", Context.MapName);
    WrapReadOnly(L); Run.MapRegistry = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushinteger(L, Context.SelfId); lua_setglobal(L, "self_id");
    PushText(L, Context.Event); lua_setglobal(L, "event");
    PushText(L, Context.EventCondition); lua_setglobal(L, "event_condition");
    lua_pushstring(L, Context.bDM ? "dm" : "player"); lua_setglobal(L, "role");
    lua_pushcfunction(L, GetToken); lua_setglobal(L, "token");
    lua_pushcfunction(L, GetMap); lua_setglobal(L, "map");
    lua_pushcfunction(L, SetAttribute); lua_setglobal(L, "set_attr");
    lua_pushcfunction(L, AddCondition); lua_setglobal(L, "add_condition");
    lua_pushcfunction(L, RemoveCondition); lua_setglobal(L, "remove_condition");
    lua_pushcfunction(L, Log); lua_setglobal(L, "log");
    lua_pushcfunction(L, Roll); lua_setglobal(L, "roll");
    Run.Deadline = FPlatformTime::Seconds() + 0.1;
    lua_sethook(L, InstructionHook, LUA_MASKCOUNT, HookInterval);
    const int Loaded = luaL_loadbufferx(L, Run.Source.GetData(), Run.Source.Num() - 1, "tabletop", "t");
    if (Loaded != LUA_OK) return lua_error(L);
    lua_call(L, 0, 0);
    return 0;
}
}

FScriptResult FTabletopScriptRuntime::Run(const FString& Source, const FScriptContext& Context)
{
    FRunState Run;
    if (Source.Len() > MaxSourceLength || Context.Tokens.Num() > 512 || Context.MapAttributes.Num() > 64)
    {
        Run.Result.Error = TEXT("Script source or snapshot exceeds its limit.");
        return Run.Result;
    }
    for (TCHAR Character : Source)
    {
        if (Character == 0)
        {
            Run.Result.Error = TEXT("Script source must not contain null characters.");
            return Run.Result;
        }
    }
    for (const auto& Token : Context.Tokens)
    {
        if (Token.Attributes.Num() > 64 || Token.Conditions.Num() > 64 || Token.Name.Len() > 80)
        {
            Run.Result.Error = TEXT("Token snapshot exceeds its limit.");
            return Run.Result;
        }
    }
    Run.Context = &Context;
    Run.Random.Initialize(Context.RandomSeed);
    CopyUtf8(Source, Run.Source);
    if (Run.Source.Num() - 1 > MaxSourceLength)
    {
        Run.Result.Error = TEXT("Script source exceeds 16 KiB of UTF-8 text.");
        return Run.Result;
    }
    lua_State* L = lua_newstate(BoundedAllocate, &Run);
    if (!L) { Run.Result.Error = TEXT("Unable to allocate script state."); return Run.Result; }
    lua_pushcfunction(L, Execute);
    const int Status = lua_pcall(L, 0, 0, 0);
    if (Status == LUA_OK) Run.Result.bSuccess = true;
    else
    {
        const char* Error = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "script failed";
        Run.Result.Error = FString(UTF8_TO_TCHAR(Error)).Left(512);
        Run.Result.Commands.Reset();
        Run.Result.Logs.Reset();
    }
    lua_close(L);
    return MoveTemp(Run.Result);
}
}
