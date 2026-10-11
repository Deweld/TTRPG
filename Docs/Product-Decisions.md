# Product decisions

Accepted by the owner on October 4, 2026. This document records clarifications
to the original outline; proposed implementation choices are identified below.

## Accepted direction

- Milestone 1: a DM hosts, a player joins, role-appropriate tools appear, the
  player edits a sheet, rolls dice, and moves an owned token; the DM can save
  and reload the campaign.
- Begin with a top-down 2D tabletop. The long-term target includes TaleSpire-like
  3D interaction and an optional VR interface selected by each user.
- Plan for one app for both DMs and players. A premium purchase for multiple DM
  campaigns is a possibility, not a finalized pricing or entitlement specification.
- Players receive only the campaign files/data relevant to them and can modify
  their local copy while offline.
- Offline changes are submitted to the DM for approval. The DM has filters and
  a change log, with DM-configurable distinctions between minor and major edits.
- The repository stores code and structural design; the paired local Unreal
  project provides the working development environment.
- GPT-assisted development should automate reasonable routine operations.

## Initial engineering defaults

- Windows and mouse/keyboard first, using the installed UE 5.8.3 baseline.
- Keep the checkout inside the temporary host's GPT directory. Load product
  plugins directly from it instead of duplicating source files.
- DM-hosted authoritative sessions; start with direct/LAN connectivity before
  adding Steam session discovery and transport integration.
- System-neutral records with a small sample ruleset rather than hard-coded
  commercial TTRPG rules.
- Submission envelopes carry a campaign ID, stable submission ID, and base
  revisions. The receiving host supplies the authenticated author identity.
- All edits require manual review by default. The DM may opt specific fields
  into automatic acceptance when permissions and revisions still match.
- Stale submissions are flagged for reconciliation; no silent last-writer-wins
  overwrite. Start with conservative record-level conflicts.
- Add bounded property/dice macros later. Arbitrary native or shell execution
  is outside the initial scripting scope.
- Commit and push development branches, with PR review before main changes.

These defaults can be revised as the first playable workflow is tested.

## Open decisions that do not block the foundation

- Exact free/premium limits, purchase enforcement, and Steam distribution setup.
- Initial sample sheet fields and the first external import format.
- Maximum lobby size and supported minimum hardware.
- Which fields should be considered minor in the default sample ruleset.
- Long-term asset storage and the permanent host project's name/location.
- Whether GPT is also wanted inside the product; currently only the development
  workflow is assumed to use it.

## Deferred features

Workshop publishing, broad imports, a visual macro editor, production Internet
connectivity, 3D scene authoring, and VR are later milestones. None is required
to verify the first hosted 2D campaign session.

## October 6 prototype expansion

The owner requested multiple DM maps with individual player assignment, per-token
vision with walls/lights, image-backed square/hex grids, movement/turn limits,
and a basic player sheet. These are implemented first in the local interface
prototype for hands-on review, keeping multiplayer acceptance unchanged.

Implementation defaults for this basic version:
- Two selectable local player identities; each is assigned to one map at a time.
  Transferring an identity moves all owned tokens, and is deferred until combat
  has ended on the source and destination maps.
- Map dimensions and grid scale are independent. Square diagonal distance is
  Euclidean; hex adjacency costs one distance unit times scale.
- Cell-based sight combines the selected player's owned characters. Walls block
  light and sight; darkness, directional cones, ranges, and darkvision are
  configurable. There is no explored fog memory yet.
- Out-of-combat distance limits are cumulative until the DM refreshes them.
  Combat movement uses sheet speed and the optional stricter map cap; exceptions
  retain that cap. DM repositioning is unrestricted by turns/distance.
- System-neutral sheet: name, HP, max HP, defense, speed, notes. Max HP, defense,
  and speed are DM-controlled. This field set can change when a ruleset is chosen.
- PNG/JPEG backgrounds are managed copies under the host Saved directory, not
  repository assets. Local session format v2 imports the previous map format
  without overwriting the original file.

These are prototype engineering choices, not decisions about final campaign
limits, player licensing, initiative rules, or network authority. Exact ruleset,
party/shared vision policy, cross-map transfers during combat, initiative editing,
and more complete sheet fields remain open.

## October 11 workspace and scripting overhaul

The owner requested explicit entry roles and separate Map Editor, Game Play,
Token Edit and Script workspaces. These expand the local prototype without
changing the hosted-session acceptance scenario.

Accepted feature direction:
- Entry as DM or player; only the DM can preview a player's view and return.
- Map preparation tools grouped around layout, tokens, lighting and triggers,
  including new, duplicate and delete map operations.
- Game Play can lock map editing and present macros, reference links, conditions,
  status bars, initiative and movement review controls.
- Token defaults are distinct from instance properties, ownership and associated
  sheets. Bars bind to attributes rather than hard-coded resource names.
- Scripts use a common language inside a restricted runtime, can inspect
  role-appropriate attributes, and can bind to map or token events.
- The owner accepted Lua and asked for simple, easy-to-learn scripting. Provide
  one-line starter actions and an inline reference before more complex examples.
- Players on their turn may propose forced movement of others for DM approval.

Implementation defaults for this iteration:
- A role entry lasts until the prototype restarts. DM player-preview is reversible;
  standalone Player entry has no DM switch. This is a local workflow boundary,
  not an authenticated multiplayer identity provider.
- Game Play begins with layout editing locked. Map Editor permits DM authoring;
  gameplay event scripts do not fire while preparing the map.
- Duplicating a map copies layout, links and unowned tokens, resets combat, and
  leaves player characters and scripts on the original. Delete requires a local
  confirmation and refuses the final map, assigned players or active combat.
- One session-wide token creation template stores sheet values, attributes,
  bars and vision. Existing instances change only on explicit apply. Ownership,
  position, conditions, initiative and movement are instance values.
- System-neutral conditions are labels; mechanics come from the chosen ruleset
  or explicit scripts. Owners can update current bars on their own tokens while
  their maxima and mechanical fields remain DM-controlled.
- Combat sorts by descending initiative, then token ID for ties. Mid-combat
  initiative edits preserve the active actor. Forced-move requests are explicit,
  bounded proposals and revalidate current state at DM approval. They do not
  calculate ruleset-specific shove range, strength or attack consequences.
- Lua 5.4 source is bundled under its upstream license. The runtime exposes
  bounded attribute/condition commands and read-only snapshots, with no OS shell,
  files, network, package loading, native handles or Unreal object access. Scripts
  have source, memory, instruction, command and log limits.
- DM-authored automatic events are `move`, `turn_start` and `condition_added`;
  manual scripts are macros. Scripts have map and optional token scope, an
  enabled flag and an optional required token condition. Player-authored scripts
  are manual. A player invoking a shared macro uses player permissions.
- Local DM entry supplies the automatic-event host context, including DM preview
  of a player. Standalone Player entry has no authenticated host and does not run
  DM automatic events. Production multiplayer will dispatch events on its host.
- Format v3 persists new properties, links and source without executing scripts
  on load. V1/V2 remain importable. Entry authority and workspace remain runtime
  state. Pending movement approvals are cleared on load rather than replayed.

These defaults do not settle a final ruleset, token library/schema format,
production script threat model, multiplayer synchronization, trigger ordering
across network commands, script sharing/import trust, or commercial limits.
See [prototype controls](Tabletop-Prototype.md) and [Lua details](Tabletop-Scripting.md).
