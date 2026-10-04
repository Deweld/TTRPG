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
