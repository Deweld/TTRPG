# Playable local tabletop prototype

The connected Unreal host is `C:/Unreal/TestProject/TestProject.uproject`.
Open it and press **Play** on `TabletopPrototype`. For more space, use a new
editor window or the standalone launch script below. The original TopDown,
Strategy, and TwinStick template levels are preserved.

The new map is `/TTRPGCore/Maps/TabletopPrototype`. It starts a native Unreal
Slate interface: a top-down encounter board, tools, roster, token inspector,
and local activity log. The current map is drawn as a 2D UI canvas, not a 3D
scene. The editor viewport is blank before Play; the interface is created by
`ATabletopPlayerController` when the level runs. "Wayfarer" and "The Watchtower"
are placeholder interface/demo names, not a product naming decision.

## Controls

| Action | Control |
| --- | --- |
| Select a token | Click it on the board or in the left roster |
| Move a controlled token | Drag it onto a cell, or select it and click an open cell |
| Pan | Hold right or middle mouse and drag the board |
| Zoom | Mouse wheel over the board; zoom stays anchored under the cursor |
| Fit the map | Fit map button, or Home with the board focused |
| Clear selection / return to move tool | Escape with the board focused |
| Change test role | DM VIEW / PLAYER VIEW in the header |
| Place a token or object | DM: choose Player token, NPC token, or Supply crate, then click a cell |
| Edit walls | DM: Paint wall / Erase wall, then click or drag across cells |
| Change HP or roll dice | Select a controlled character, then use its inspector |
| Hide/reveal a token | DM: select a token, then use the visibility button |
| Save / restore layout | DM: Save map / Load saved map |

Movement snaps to cells. Walls, occupied cells, and map bounds prevent placement
and movement. This is free tabletop repositioning, not pathfinding or movement
budget enforcement; tokens may be moved across intervening walls into open cells.

Mira is the initial player-controlled character. Rowan is a different party
member. The hidden Lookout is visible only in DM view. Player mode removes map
editing controls, filters hidden tokens out of drawing/selection/roster, and
blocks edits to tokens the player does not control. A role switch clears the
local activity panel so DM actions do not appear in the player view.

The role buttons intentionally let one person test both interfaces. These are
local prototype restrictions, not a security boundary or authenticated session.
No shared/network dice log or offline campaign approval UI is implemented here.

## Persistence

Save map writes only this prototype's layout, tokens, visibility, and HP to the
host's `Saved/TabletopPrototype/Map.json`. It uses a temporary file before
replacement; the reader validates version, fields, IDs, bounds, and occupancy
before replacing the current state. Load is explicit and does not happen at
startup. Invalid saves preserve the active layout. Activity, selection, role,
and camera position are not saved. This is separate from the planned campaign
save format and offline proposal ledger.

## Reproduce in another connected host

Close the editor before compiling. Use PowerShell 7 from this checkout:

```powershell
./Scripts/Connect-UnrealProject.ps1 -ProjectPath ../TestProject.uproject
./Scripts/Build.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8 -Target TestProjectEditor -RefreshModules
./Scripts/Prepare-Tabletop.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
./Scripts/Launch-Tabletop.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
```

`Prepare-Tabletop` creates the plugin map if missing, preserves an existing map,
and backs up the host's `DefaultEngine.ini` before changing its startup map.
`-RefreshModules` refreshes the descriptor timestamp so Unreal discovers changed
plugin modules instead of reusing its module cache. Use it after adding/removing
modules; ordinary code edits do not require it.

No paid assets, downloaded art, additional plugins, or external services are
required. The small `.umap` is tracked as a binary alongside its reproducible
editor commandlet. The UI and sample arrangement are authored in C++.
