# Foundation validation

Verified October 4, 2026 against the local `TestProject` host.

- Unreal Engine 5.8.3 (58210709), Windows Win64 Development editor target.
- UnrealBuildTool selected Visual Studio MSVC 14.44.35229 and Windows SDK
  10.0.22621.0. The plugin and host editor target built successfully.
- `Connect-UnrealProject.ps1` preserved the original host descriptor, connected
  `GPT/Plugins`, and enabled `TTRPGCore`. A second invocation made no changes.
- All 8 `TTRPG.Review.*` automation tests passed, with 0 failures and 0 not run:
  atomic proposal application, current permissions, invalid proposals, manual
  review by default, minor-change policy, rejection, replay protection, and
  stale revision detection.
- Test reports remain local in the host's `Saved/TTRPGAutomation` directory;
  they are generated artifacts and are not committed.

The build initially failed under the restricted execution sandbox. Running the
same build with approved access to Unreal's engine/per-user tooling succeeded.
Headless automation likewise used approved execution outside the sandbox.

These results validate the in-memory domain rules and integration with this
host. They do not validate networking, disk persistence, a packaged game, UI,
3D/VR presentation, or the full Milestone 1 acceptance scenario.

## Local tabletop prototype

Verified October 4, 2026 with the same UE 5.8.3 editor target:

- Compiled the new `TTRPGTabletop` runtime and `TTRPGTabletopEditor` editor modules.
- Generated the plugin's `TabletopPrototype.umap` and connected it as the host's
  editor/game startup map, preserving the previous configuration in a local backup.
- All 12 automation tests passed: the original 8 review tests plus 4 tabletop
  tests for player permissions, grid/placement/stroke behavior, layout round-trip,
  and invalid-save rejection without replacing current state.
- Opened the native standalone Unreal window and inspected the DM/player
  layouts, token rendering, placement, and save/load. Live visual checks caught
  and fixed oversized token corner radii and missing cells in fast wall strokes.
- Saved the owner's in-progress layout before restart and preserved a second
  local copy. The save and backup hashes matched after testing. Personal layout
  data is not committed.
- Corrected the initial Slate keyboard focus target; no runtime errors appeared
  in the updated prototype log during the checked session.

This verifies a local interaction prototype, not authenticated multiplayer,
production campaign persistence, packaging, or 3D/VR. See Tabletop-Prototype.md
for the exact scope and controls.

## Maps, vision, movement, and sheets

Verified October 6, 2026 with UE 5.8.3 / Win64 Development editor:

- Host/plugin build succeeded after the final UI changes.
- All 18 automation tests passed (8 campaign review, 10 tabletop), with no
  failures or skipped tests. The six additional tests cover independent map
  assignments and failed-transfer atomicity; square/hex geometry and paths;
  directional sight, walls, light and darkness; cumulative movement, combat,
  and exceptions; sheet authority; and v2 restoration / v1 migration / invalid
  relationship and image-path rejection.
- Final local report: `Saved/TTRPGAutomation/f0c22640e6aa4a2bb5537720040eb3ef/index.json`.
- Live native Slate checks verified square and hex rendering, transferring Mira
  while Rowan stayed on the original map, player-specific map lists and fog,
  token dragging, visible sheet permission differences, PNG and JPEG background
  import, combat advancement and out-of-turn rejection, and session save/reload
  with the managed image and combat state restored.
- Live verification caught a first-frame polygon-array assertion that headless
  tests did not exercise; corrected it and successfully relaunched. Reduced tab
  padding after seeing clipped labels in the native window. No runtime errors
  appeared in the subsequent checked interactive session.
- The original owner's Map.json remains byte-identical to its earlier backup.
  Test session/artwork stay local and untracked. Loading the original layout
  uses the migration path; it is never overwritten by the v2 save writer.

Remaining verification: packaged builds, hardware/performance limits, networked
permissions and replication, cross-machine asset transfer, and the complete
Milestone 1 two-instance workflow. Lighting here is discrete cell visibility,
not Unreal scene lighting or 3D/VR rendering.
