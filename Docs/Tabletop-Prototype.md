# Playable local tabletop prototype

The connected Unreal host is `C:/Unreal/TestProject/TestProject.uproject`.
Open `/TTRPGCore/Maps/TabletopPrototype` and press **Play**, or use the standalone
launch script below. The editor viewport is blank before Play: the player
controller creates the native Slate interface at runtime. The 2D board,
"Wayfarer," and sample map/character names remain prototype presentation.

## Entry and workspaces

Choose **DM View** or **Player View** when entering. That choice lasts for the
running session. A DM can preview the selected player's view and return to DM.
A Player entry cannot become DM or select a different player's identity. Restart
the prototype to make a new entry choice. This prevents accidental role changes
in the local interface; authenticated network sessions are still future work.

| Workspace | Purpose |
| --- | --- |
| Map Editor | Prepare maps, layouts, tokens, images, lighting and event bindings. |
| Game Play | Run the map with macros, links, token status and combat controls. |
| Token Edit | Edit the selected instance, ownership, associated sheet and defaults. |
| Script | Author bounded Lua macros and map/token event scripts. |

Map Editor is DM-only. Game Play starts with map editing locked; the DM can
unlock layout tools when needed. Moving controlled tokens and changing gameplay
status remain available while layout editing is locked. Moving a token while
preparing a map does not run gameplay triggers. Script mode exposes only the
macros a player is permitted to read or edit.

## Maps and player assignments

The map library lists the DM's maps, each with independent dimensions, grid,
walls, lights, background image, movement rules, combat state and links. Create
up to 16 maps, each 4–64 columns and rows. Safe resize refuses to discard walls,
lights or tokens. The square Watchtower and hex Lantern Cavern are sample maps.

A DM chooses the local player identity, opens the destination map, and brings
that player there. All of that player's owned characters move to free cells;
other players remain on their maps. DM browsing does not move anyone. A full
map refuses the transfer atomically. End combat on source and destination maps
before transferring. Player view follows only its assigned map. The two named
identities remain local test participants, not user accounts.

**Duplicate** copies a map's layout, image reference, lights, links and unowned
tokens to new IDs. Combat and movement state reset; player-owned characters and
scripts stay on the original map. Imported images are shared by reference, not
copied or deleted. Review the copy and add its event bindings explicitly.

**Delete** asks for confirmation in the interface. It is refused for the final
map, an active combat map, or a map with assigned players. Move players away
first. Deletion removes the map's token instances, scripts and local movement
proposals, while leaving managed image files available.

## Board controls

| Action | Control |
| --- | --- |
| Select | Click a visible token or its roster entry. |
| Move | Drag a controlled token, or select it and click an open cell. |
| Request forced movement | In combat on your turn, attempt to drag another visible token; DM review decides whether it moves. |
| Pan | Hold right or middle mouse and drag. |
| Zoom | Mouse wheel over the board, anchored under the cursor. |
| Fit map | Fit map button; Home while the board is focused. |
| Return to Select | Escape while the board is focused. |
| Place character/NPC/crate | With map editing enabled, select a placement tool and click a cell. |
| Draw/erase blocking walls | With map editing enabled, select the wall tool and click or drag. |
| Add/change a light | Set the light range, select the light tool and click a cell. |

New player characters use the selected identity when it is assigned to this map.
Other token ownership can be set explicitly in Token Edit. Tokens cannot overlap
walls, each other or map bounds. Lights may share a token's cell, but not a wall.
The local session limit is 512 tokens and 32 lights per map.

## Grid and background image

Choose square or pointy hex cells per map. Changing the grid preserves column/row
coordinates and changes their geometry. **Distance units per cell** applies to
movement, vision and lights; it does not change the number of cells. The default
is 5 system-neutral units. Square diagonals cost sqrt(2) times the scale; each
adjacent hex costs one scale unit. Player paths respect walls, occupied cells and
square corner blocking.

In map settings, enter a local PNG/JPEG path and import it. The prototype makes
a managed copy in the host's `Saved/TabletopPrototype/Images` directory and draws
the grid over it. Images initially stretch to map extent. Scale, X/Y offsets and
grid opacity help align existing artwork. Returning to the block background
removes the reference without deleting the file.

Imports are limited to 32 MB compressed, 16 megapixels decoded and 8192 pixels on
either side. Only PNG/JPEG contents are accepted. These images remain local;
they are not repository assets, JSON-embedded data, or Unreal Content imports.

## Vision and lighting

Enable dynamic token vision in map settings. Token vision settings provide range,
cone width and darkvision; 360 degrees is all around. The owner can change facing:
0 degrees east, 90 south, 180 west, 270 north. Player sight combines the current
view of that player's characters. Hidden tokens remain DM-only, and each owned
token's own cell stays visible for selection.

With darkness enabled, cells in sight also require a wall-unobstructed light,
unless the viewer has darkvision. With darkness disabled, ambient light fills
the map. Enable dynamic vision for player lighting restrictions to apply. DM
view sees the full board and dims unlit cells when both settings are enabled.

This is discrete cell-based sight, not Unreal 3D lighting. Explored fog memory,
soft falloff, colored lights, doors and party-shared vision remain future work.

## Game Play, initiative and movement review

Outside combat, players move their own characters along open paths. The DM may
also configure a cumulative map distance allowance; multiple drags spend the same
budget. Refresh movement allowances to reset it. With that limit disabled,
out-of-combat movement is unrestricted by distance.

Starting combat sorts characters by initiative, highest first; token ID breaks
ties. Crates do not take turns. Editing initiative during combat reorders the
tracker while preserving the active character. Next turn advances the tracker
and refreshes the incoming character's movement budget. Player movement is limited
to that character's turn and sheet speed, optionally capped further by the map
allowance. A DM out-of-turn exception retains the distance cap until cleared.

On their active turn, a player may propose moving another visible token, such as
a push. The target stays in place until the DM approves. The DM sees pending
requests and can approve or reject them. Approval rechecks the acting player,
turn, source/target positions and ownership, destination and current board. A
stale request is rejected rather than applied to changed state. The prototype
does not calculate push strength, opportunity attacks or other ruleset mechanics.

DM repositioning bypasses turn and distance budgets, but still respects bounds,
walls and occupied destinations. Hidden blockers return a generic unavailable
message rather than disclosing the hidden token. Game Play also lists permitted
manual macros and map links; only HTTP/HTTPS links are accepted. Opening a link
is an explicit user action. DM trigger diagnostics stay out of player view.

## Token Edit, sheets, conditions and bars

The basic sheet contains name, current/max HP, defense, speed and notes, plus a
d20 roll and remaining movement. The owner may edit name, current HP and notes;
mechanical maxima, defense and speed remain DM-controlled. Other players' visible
sheets are read-only. Notes and rolls are local; there is no shared network log.

DM can set ownership, numeric custom attributes, conditions, initiative and status
bar bindings per instance. Up to four bars bind a label to current and maximum
attributes, such as HP/max HP or stamina/max stamina. Owners may update a bound
current value on their token, but cannot use the bar editor to raise its maximum
or movement speed. Conditions are system-neutral labels; they do not implicitly
change rules or movement.

Capture the selected token as creation defaults to reuse its sheet, attributes,
bars and vision settings for new tokens. Existing instances keep their properties
until the DM explicitly applies defaults to one. Ownership, location, conditions,
initiative and spent movement are instance state, not creation defaults. This is
one session-wide creation template, not a full token asset library.

## Script workspace

The Script editor stores Lua 5.4 source with a name, map scope, optional token
scope, enabled flag, event and optional required token condition. Event names are
`manual`, `move`, `turn_start` and `condition_added`. Manual scripts appear as
macros. Automatic events run only during Game Play, including DM player-preview,
and only for DM-authored scripts. Map Editor is a preparation space.

A DM may publish a manual macro for player use. Player-authored macros are manual
and execute under that player's current permissions. Published DM macros also
use the invoking player's permissions when a player runs them. Scripts inspect
only the tokens/attributes supplied by their current role; property changes are
validated and applied together after successful execution.

The local DM entry supplies the host context for automatic triggers. A standalone
Player entry has no authenticated host connection and does not run DM automations.
Lua has no OS shell, filesystem, process, network, package-loading or Unreal object
access. Source, memory, instruction, command and log budgets bound each run. Each
run starts with a fresh Lua state. See [the Lua guide](Tabletop-Scripting.md) for
API examples, exact limits and the bundled source license.

## Save/load and current boundary

**Save session** writes `Saved/TabletopPrototype/Session.json` through a temporary
file. Version 3 preserves maps, player assignments, sheets, vision, managed image
references, spent movement, combat order, token defaults, instance properties,
conditions, bars, links and script source. Loading validates all supported fields
and relationships before replacing the current board. Invalid data leaves it
intact. Loading never compiles or executes saved scripts.

Version 1 layouts and version 2 sessions migrate in memory. If Session.json is
absent, Load tries the earlier `Map.json`; saving then writes Session.json and
leaves that original file untouched. Back up Session.json and the Images folder
together. No save is loaded automatically at startup.

Entry authority, current role/workspace, edit lock, callbacks and camera are not
restored from files. Selection and event logs reset. Pending forced-move approvals
are transient and clear on successful load; they cannot replay against a reloaded
board. This local JSON is not the durable campaign proposal/decision ledger.

The prototype remains single-instance. Network host/join, authenticated commands,
visibility-filtered network payloads and offline player submissions are not wired
into these controls. The separate campaign-review domain remains the foundation
for that work. No in-game AI account, API service, upload or paid dependency is
required.

## Reproduce in another connected host

Close the editor before compiling. From this checkout, using PowerShell 7:

```powershell
./Scripts/Connect-UnrealProject.ps1 -ProjectPath ../TestProject.uproject
./Scripts/Build.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8 -Target TestProjectEditor -RefreshModules
./Scripts/Test.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
./Scripts/Prepare-Tabletop.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
./Scripts/Launch-Tabletop.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
```

`Prepare-Tabletop` preserves the existing map and backs up host configuration.
`-RefreshModules` refreshes source/module discovery when needed. Lua source is
bundled with its license and builds locally; no download or interpreter install
is needed. See [validation](Validation.md) for actual build/test results.
