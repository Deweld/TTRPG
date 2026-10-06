# Playable local tabletop prototype

The connected Unreal host is `C:/Unreal/TestProject/TestProject.uproject`.
Open `/TTRPGCore/Maps/TabletopPrototype` and press **Play**, or use the standalone
launch script below for more room. The original template levels are preserved.
The editor viewport is blank before Play: `ATabletopPlayerController` creates the
native Slate interface at runtime. The board is a 2D canvas. "Wayfarer" and the
sample map/character names are placeholders.

## Maps and player assignments

**Maps** lists the DM's maps, with independent dimensions, grid type, walls,
lights, image, and movement/combat settings. Create up to 16 maps, each 4–64
columns and rows. **Settings** renames/resizes the selected map. Shrinking is
refused when it would discard walls, lights, or tokens.

Choose **Mira** or **Rowan** in the header to select a local test identity. As DM,
open a map and use **Bring this player to this map**. That player and all of their
owned characters move to free cells, while the other player remains on their
map. The DM can reposition arriving tokens. A full destination refuses the
transfer without changing anything. End combat on both maps before transferring.
DM browsing alone does not move players. **PLAYER VIEW** follows the selected
identity's assigned map; its list contains only that map.

Fresh sessions contain Watchtower (square) and Lantern Cavern (hex, darkness and
vision enabled). Loading an old single-map save imports only that saved map;
create additional maps after loading. The two identity buttons are local test
controls, not account creation or authentication.

## Board controls and tools

| Action | Control |
| --- | --- |
| Select | Click a token, or its entry under Tools → Visible tokens |
| Move | Drag a controlled token, or select it and click an open cell |
| Pan | Hold right or middle mouse and drag |
| Zoom | Mouse wheel over the board, anchored under the cursor |
| Fit map | Fit map button; Home while the board is focused |
| Return to Select / move | Escape while the board is focused |
| Place character/NPC/crate | DM: choose the corresponding Tools button, then a cell |
| Draw blocking walls | DM: Paint blocking wall / Erase wall, then click or drag |
| Add/change a light | DM: set New light range, choose Place / update light, click a cell |
| Remove a light | DM: choose Remove light, then click the light |

New player characters belong to the selected test identity; assign that identity
to the current map first. Tokens cannot overlap walls, each other, or map bounds.
Lights can share a token's cell but cannot share a wall cell. There are at most
512 tokens across the session and 32 lights per map.

## Grid and image settings

Each map can use **square** or **pointy hex** cells. Switching grid type keeps
column/row coordinates and changes their geometry. **Distance units per cell**
sets the scale for movement, vision, and light ranges; it does not change the
number of cells. The default is 5 units, with no ruleset-specific unit assumed.
Square diagonals cost sqrt(2) times the scale; every adjacent hex costs one scale
unit. Walls and tokens block player paths, including square corner cutting.

Paste a local PNG/JPEG path into **Settings → Background image**, then click
**Import image**. The prototype copies the image into the host's local
`Saved/TabletopPrototype/Images` directory. The grid is overlaid on it. Images
initially stretch to the map extent; use image scale and X/Y offsets to align
existing artwork, and grid opacity to adjust contrast. **Use block background**
removes the reference from the map without deleting the imported file.

Imports are limited to 32 MB compressed, 16 megapixels decoded, and 8192 pixels
on either side. Only PNG/JPEG contents are accepted. Images are not committed,
uploaded, embedded in the JSON save, or imported as Unreal Content assets.

## Vision and lighting

Enable **Dynamic token vision** in map Settings. Walls then block player sight.
Under **Vision**, DM configures each token's range, cone width (360 is all around),
and darkvision. The owner can change facing: 0° east, 90° south, 180° west, 270°
north. Player view combines the current sight of all that player's characters.
Hidden tokens remain DM-only. The token's own cell stays visible for selection.

With **Darkness / require light**, visible cells also need a wall-unobstructed
light within range unless the character has darkvision. With darkness off, ambient
light fills the map. Enable dynamic vision for player light restrictions to apply.
DM view shows the whole map, dimming unlit cells when both options are enabled.

This is discrete, cell-based line of sight and lighting, not an Unreal 3D lighting
simulation. There is no explored fog memory, soft light falloff, movable doors,
colored lights, or shared party vision yet.

## Movement and combat

Enable **Limit player movement** and set **Distance allowance** for a cumulative
map allowance outside combat. Multiple short drags spend the same allowance.
The DM's **Refresh movement allowances** resets it. Turning that option off makes
out-of-combat movement unrestricted, while still requiring an open path.

In **Combat**, **Start / end combat** builds a turn order from characters in roster
order (crates excluded). **Next turn** advances and refreshes the incoming token's
movement. Players can move only their own active character. The maximum is the
character sheet's movement speed, further capped by the map limit when enabled.
DM's **Selected token: out-of-turn exception** allows that token to move outside
its turn, still spending its allowance. The exception stays enabled until DM
clears it. The tracker has no initiative rolls/reordering yet.

DM repositioning ignores distance and turn restrictions, but still respects bounds,
walls, and occupancy. Player destinations must currently be visible. Pathfinding
uses all occupied cells; unknown blockers produce a generic unavailable message.

## Basic character sheet

Select a character and open **Sheet**. Fields are name, current/max HP, defense,
movement speed, and notes, with a d20 roll button and remaining movement display.
Edits commit on Enter or focus change; numeric values also respond to dragging.
The owner can edit name, current HP, and notes. Maximum HP, defense, and speed are
DM-controlled. Another player's visible sheet is read-only. Vision settings are
on the adjacent Vision tab. This is system-neutral and does not calculate
abilities, attacks, inventory, or ruleset modifiers.

## Persistence and current boundary

**Maps → Save session** writes `Saved/TabletopPrototype/Session.json` using a
temporary file before replacement. **Load saved session** restores maps, player
assignments, sheets, vision, images references, movement spent, and combat turns.
Load is explicit; nothing loads automatically at startup. The versioned reader
validates bounds, IDs, relationships, image filenames, and occupancy before
replacing current state. Invalid files leave current maps intact.

If Session.json does not exist, Load falls back to the previous `Map.json` and
migrates its layout/HP/ownership in memory. Saving then writes Session.json and
leaves the original Map.json untouched. Back up both Session.json and Images to
keep an expanded session. Activity/status, role, selection, and camera are not
persisted.

This remains a single-instance interaction prototype. Role/identity switches are
for testing and are not a security boundary. Network host/join, visibility-filtered
network payloads, shared dice history, and offline sheet review are not connected
here. The separate campaign-review domain tests continue to run. These local save
records do not replace the planned campaign persistence and proposal ledger.

## Reproduce in another connected host

Close the editor before compiling. Use PowerShell 7 from this checkout:

```powershell
./Scripts/Connect-UnrealProject.ps1 -ProjectPath ../TestProject.uproject
./Scripts/Build.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8 -Target TestProjectEditor -RefreshModules
./Scripts/Test.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
./Scripts/Prepare-Tabletop.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
./Scripts/Launch-Tabletop.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
```

`Prepare-Tabletop` preserves an existing map and backs up the host configuration
before changing its startup map. `-RefreshModules` refreshes module discovery
when adding/removing modules; ordinary edits do not require it. The plugin map
is tracked alongside its reproducible editor commandlet. No paid assets,
external services, or additional plugin installations are required.
