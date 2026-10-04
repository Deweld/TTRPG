# TTRPG

A system-neutral virtual tabletop built in Unreal Engine. The DM owns the
authoritative campaign; players use configurable tools and may prepare changes
offline for DM approval.

The initial presentation is a top-down 2D tabletop. The architecture should allow
a later TaleSpire-like 3D view and optional per-user VR controls without replacing
the campaign model. One application serves players and DMs; premium campaign
capacity is a proposed commercial model, with exact entitlements still undecided.

## Repository and Unreal workspace

This repository is the source of truth for product code, design, and reusable
authored assets. The current Unreal template is a disposable development host:

```text
TestProject/
  TestProject.uproject       # Local Unreal host; not part of this repository
  Content/                  # Existing template content
  Source/                   # Existing template module
  GPT/                      # This Git checkout
    Overview/               # Original product outline
    Docs/                   # Accepted decisions, architecture, milestones
    Plugins/TTRPGCore/      # Product code, built directly by the host
    Scripts/                # Repeatable host setup, build, and tests
```

There is no code copy/synchronization step. `Connect-UnrealProject.ps1` adds this
checkout's `Plugins` directory to the host's `AdditionalPluginDirectories` and
enables `TTRPGCore`. The script preserves a descriptor backup before changing it.
Run it again when moving the checkout or using a different Unreal host.

Keep product-specific assets under plugin `Content` directories so they travel
with the repository. The current template's maps and assets remain in the local
host. Before adding substantial binary content, select Git LFS or another versioned
asset store and validate a clean restore. A complete distributable host project
will be added before packaging; this initial checkout is not a standalone game.

## Development

Current baseline: Unreal Engine 5.8.3, Windows, Visual Studio C++ tools.
From this checkout, using PowerShell 7:

```powershell
./Scripts/Connect-UnrealProject.ps1 -ProjectPath ../TestProject.uproject
./Scripts/Build.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8 -Target TestProjectEditor
./Scripts/Test.ps1 -ProjectPath ../TestProject.uproject -EngineRoot D:/EpicGames/UE_5.8
```

Engine and project paths are arguments, not product-code constants. Close the
editor before a full build. Tests run headlessly and store reports in the host's
`Saved/TTRPGAutomation` directory. Build before testing after code changes.

Read [the accepted direction](Docs/Product-Decisions.md),
[the architecture](Docs/Architecture.md), and
[the first milestone](Docs/Milestone-1.md).

## Current implementation

`TTRPGCore` is a runtime plugin with an in-memory campaign change-review model
and Unreal automation tests. It handles owned record edits, DM review policy,
revision conflicts, atomic application, and duplicate submissions. It does not
yet implement disk storage, networking, Steam, sheets UI, or a playable tabletop.

Work takes place on development branches, with pull requests for review. The
original `Overview/Outline.txt` is retained; newer accepted decisions are recorded
explicitly in `Docs/Product-Decisions.md`.
