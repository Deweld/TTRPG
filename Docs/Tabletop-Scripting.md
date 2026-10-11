# Tabletop scripting

The local prototype embeds [Lua 5.4.9](https://www.lua.org/manual/5.4/). It executes
ordinary Lua syntax in a fresh, restricted interpreter. Scripts have no operating
system shell, files, network, native objects, or external service requirements.
The bundled source carries the [MIT license](../Plugins/TTRPGCore/Source/TTRPGTabletop/Private/ThirdParty/Lua/LICENSE.txt).

## Start with one line

The editor has **Roll d20**, **Add Prone**, and **Inspect token** starter buttons.
Select a token, open **Script**, choose a starter, and click **Save & run macro**.
Saved manual macros also appear in **Game Play**. Starter buttons replace the
current draft's code; save work you want to keep before choosing another starter.

```lua
log(roll(20))
```

```lua
add_condition(self_id, "Prone")
```

`self_id` means the selected token unless the script is bound to a specific token.
Only add variables and `if ... then ... end` when a rule needs them. The editor's
quick reference stays visible beside the code.

## Authoring and events

Open **Script** mode to create, edit, save, or run a macro. A script belongs to one
map and can optionally bind to one token instance. Map-bound manual macros use
the selected token as `self_id`; token-bound scripts use their bound token.

DM authors can select `manual`, `move`, `turn_start`, or `condition_added`.
Automatic events run only during **Game Play**, including a DM's player preview.
A nonempty **Condition** requires that status on the event token before the
script runs. Conditions are names; they are not executable Lua expressions.
The DM can inspect trigger results without exposing those diagnostics in the
player preview.

Player authors can save personal manual macros for their map or their owned
characters. The model sets their owner identity and prevents automatic event
registration. A DM can allow players to call a DM-authored manual macro, but a
player's call still runs with that player's permissions. Source and names of
player-callable macros are visible to eligible players, so those macros must not
contain private DM information.

The local DM session acts as the prototype's host. A standalone **Player** entry
cannot execute DM automatic triggers. Connecting this behavior to a real DM host
and authenticated player identities remains part of multiplayer development.

## Globals and snapshots

| Lua value | Meaning |
| --- | --- |
| `self_id` | Bound token or selected/event token ID; no selection can be zero. |
| `event` | `manual`, `move`, `turn_start`, or `condition_added`. |
| `event_condition` | Newly added condition name for a condition event, otherwise empty. |
| `role` | Effective execution authority: `dm` or `player`. |
| `token(id)` | Read-only token snapshot, or `nil` if unavailable. |
| `map()` | Read-only active-map snapshot. |

Token snapshots have `id`, `map_id`, `owner_id`, `name`, `conditions`, and numeric
attributes. Built-in numeric keys are `hp`, `maxHP`, `defense`, `speed`, `x`, and
`y`. Numeric custom attributes are available alongside them and through `attrs`.
For example, `token(1).hp` and `token(1).attrs.hp` read the same starting HP.
`conditions` is an array of names and supports `ipairs` and `#`.

Map fields are `id`, `name`, `width`, `height`, `scale`, `round`, and `combat`
(`1` for combat, `0` otherwise). Positions use zero-based grid coordinates.

A player receives only visible tokens on the current map and their public numeric
attributes/statuses. Sheet notes, hidden tokens, other maps, scripts, image paths,
and DM logs are absent. The current custom-attribute editor defines public numeric
attributes; there is no private custom-attribute flag yet. DM snapshots include
all tokens on the active map.

Snapshots are immutable and represent the start of the run. Queued edits do not
change later reads within that run. Use local variables to calculate a sequence
of changes.

## Commands

| Lua call | Result |
| --- | --- |
| `set_attr(id, key, number)` | Propose a numeric attribute update. |
| `add_condition(id, name)` | Propose adding a status. |
| `remove_condition(id, name)` | Propose removing a status. |
| `log(text)` | Add a result-log line after a successful run. |
| `roll(sides, count)` | Return the sum of local dice; `count` defaults to `1`. |

The DM can edit authorized attributes on active-map tokens. A player can change
only owned-token HP and existing custom attributes assigned as status-bar values,
provided the attribute is not also a bar maximum. Maximum HP, defense, speed,
bar maxima, ownership, movement, map geometry, and initiative cannot be changed
by a player's script. Players may add/remove statuses on owned tokens.

Attribute keys are case-sensitive in Lua snapshots. Use the exact saved key.
Numbers must be finite and between -1,000,000 and 1,000,000, with narrower domain
limits for built-in fields. Token condition names are limited to 60 characters
(the optional trigger filter field accepts up to 64). Dice
allow 2–1,000 sides and 1–100 dice per call. Statuses are descriptive; a ruleset
must explicitly implement any associated effects.

Successful execution produces queued proposals. The model validates all proposals
against current role, ownership, and field rules on an isolated copy, then commits
all of them together. A script error, exhausted limit, or rejected command applies
none of that script's edits. Logs from failed runs are discarded. Separate automatic
triggers are separate transactions. Script commands do not recursively fire more
triggers; this prevents event loops.

### Heal the selected character

```lua
local t = token(self_id)
assert(t, "Select a character first")
local healed = math.min(t.hp + roll(6), t.maxHP)
set_attr(t.id, "hp", healed)
log(t.name .. " now has " .. healed .. " HP")
```

### Add a movement-trigger status

Create a DM-owned script with event `move`, optionally bound to one token:

```lua
local t = token(self_id)
if t and t.x >= 12 then
    add_condition(t.id, "Exposed")
    log(t.name .. " entered the eastern area")
end
```

This is a grid-coordinate example, not a full region/trigger-volume editor.
Repeatedly adding the same condition is harmless.

### Inspect a second visible character

```lua
local self = token(self_id)
local ally = token(2)
if self and ally then
    local defense = ally.defense
    log(ally.name .. " has defense " .. defense)
end
```

Availability of a snapshot does not grant write permission to that character.

## Runtime limits and supported libraries

Each invocation has at most 16 KiB of UTF-8 source, 2 MiB of Lua-managed memory,
100,000 VM instructions, 64 proposed commands, and 32 log lines. Each displayed
log line is limited to 256 characters. A time check at instruction-hook boundaries
also stops execution after approximately 100 ms; this is not a real-time deadline
for each individual native library call.

Native operations have additional work bounds because VM instruction hooks do not
run inside ordinary C library loops: `string.rep` allows at most 65,536 copies;
table insertion/removal and range operations accept at most 4,096 elements;
`table.sort` allows 256 elements and at most 64 KiB of combined string input
(counting repeated references). These operations charge the execution budget and
check the deadline before and after their native call.

Lua arithmetic, functions, loops, tables, base operations, and selected `math`,
`string`, `table`, and `utf8` functions are supported. These capabilities are absent:

- `os`, `io`, `package`, `require`, `debug`, and `coroutine` libraries.
- `load`, `loadfile`, `dofile`, bytecode loading, and `string.dump`.
- `pcall`/`xpcall`, which could otherwise catch quota termination.
- `collectgarbage`, `print`, `warn`, raw table mutation, and metatable access.
- Native-loop pattern operations `string.find`, `match`, `gmatch`, and `gsub`.
- `math.random` and `math.randomseed`; use the bounded `roll` API.

Every invocation starts with a fresh state. Globals and local variables do not
persist between runs. Save persistent game values through authorized attributes.
The execution boundary reduces capabilities and limits resources; it is not a
claim of operating-system process isolation.

## Integration contract

`FTabletopScriptRuntime` depends on copied snapshots and returns proposals. It
has no reference to a world, board model, Slate widget, or Unreal UObject. The
caller must resolve authenticated authority outside the runtime, filter readable
state before invocation, and revalidate every result before atomic application.
Neither a saved script owner nor a player's `role` variable is authentication.

Saved sessions contain script source and bindings only. Loading never executes
source. The runtime does not read a save file, load a Lua package, or execute a
file path. The implementation compiles checked-in source directly through Unreal
Build Tool; it requires no user Lua installation. The source version, upstream
URL, verified archive hash, and copyright are recorded with its bundled license.
