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
