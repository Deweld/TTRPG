# Architecture

## Boundaries

`TTRPGCore` owns system-neutral campaign records and the offline change-review
rules. `TTRPGTabletop` provides the local board model, native Slate presentation,
versioned prototype saves and Lua integration. The editor module prepares the
host map. Authenticated transport and durable campaign persistence remain future
work. Core records use stable IDs rather than actor pointers so they can survive
saves, offline copies and alternative interfaces.

A 2D token, a future 3D miniature, and a VR controller should issue commands
against the same authorized campaign state. The initial generic property model
is a scaffold; typed schema validation and units are required before importing
untrusted content or exposing arbitrary editable fields in production.

## Offline changes

1. The host exports a visibility-filtered snapshot containing only data the
   authenticated player may receive. Hidden DM data must never be sent merely
   because the client UI hides it.
2. The client preserves that approved baseline separately from its local draft.
3. A submission contains a campaign ID, stable submission ID, record IDs,
   base revisions, and field edits. It does not grant itself an author or role.
4. Transport resolves the authenticated player ID. The host checks current
   ownership/edit grants, campaign identity, revisions, fields, and DM policy.
5. An unchanged baseline plus only fields explicitly marked minor may be
   auto-accepted when the DM enables that policy. All other valid edits queue
   for review. A stale revision is a conflict even if the field is minor.
6. DM review shows before/after values, submitter, time, and reasons for flags.
   Approval revalidates against current state. Milestone 1 applies the whole
   submission atomically; field-level selective review is a later extension.
7. Persist the decision and new campaign state together, then acknowledge it.
   Retries of a decided submission do not apply it again. Rejected or conflicting
   drafts remain available to the player for correction and resubmission.

Approval must come from an authenticated DM command, not a boolean trusted from
the network. The current in-memory API receives a trusted host decision enum;
session integration is responsible for enforcing that boundary. Approved or
rejected IDs are remembered for the life of the in-memory campaign; persistence
must preserve that decision ledger before cross-restart replay protection exists.

## Campaign review versus local tabletop

The campaign review domain implements existing-record property edits, ownership/edit grants,
record-level revision checks, a DM field allowlist for minor changes, explicit
approval/rejection, atomic in-memory application, a before/after decision log,
and duplicate-ID rejection after a decision.

That domain still needs authenticated sessions, visibility-filtered export, a
durable pending queue/decision ledger, campaign disk transactions, typed sheet
schemas, record creation/deletion and its offline-review UI. Its tests exercise
the rules directly and do not establish a working multiplayer or offline UI.

The separate local board model already owns maps, assignments, token instances,
visibility, movement/combat rules and reviewed forced moves. Slate is an adapter
over those rules; it does not own a second campaign state. Domain methods check
ownership and current permissions rather than relying only on disabled buttons.
The local board is not yet wired to the campaign review model or network commands.

## Local roles and workspace rules

Entry chooses DM or a local player identity once for the running prototype. DM
entry can preview a player and return; Player entry cannot elevate itself through
the view switch. Effective DM operations require both the current role and the
entry authority. Role selection remains local, not externally authenticated.

Workspace and layout edit lock are presentation/workflow state. Map Editor allows
DM layout edits; Game Play can unlock them explicitly. Token Edit handles defaults,
instance properties and ownership. Script mode edits permitted source. Preparation
does not emit automatic gameplay effects. These workspace choices are not saved
as authority and cannot be granted by loading campaign content.

Forced moves are transient proposals containing actor, target, author identity,
map, source positions, target ownership, destination and the map's turn generation.
DM approval rechecks current authority, turn, visibility, occupancy and reachability.
Changed premises invalidate the request. Per-map turn generations keep independent
combats separate. A decided proposal cannot apply twice. Loading clears proposals;
this queue is not the persistent offline submission ledger.

## Token records and local save format

A token instance contains the associated system-neutral sheet, custom numeric
attributes, condition labels, initiative and attribute-bound status bars. A
session-wide creation template supplies sheet/attribute/bar/vision defaults.
Capturing defaults does not mutate existing instances, and ownership and location
remain instance data. This structure can later feed 2D, 3D or VR representations.

`TabletopPersistence.cpp` writes local format v3 and migrates v1/v2 in memory.
It parses into an isolated candidate, validates counts, finite numeric ranges,
IDs, occupancy and cross-record references, then replaces state only after all
checks pass. Scripts are source records, not execution instructions for the
loader. Script names, authors, events and map/token scopes are validated; loading
never invokes scripts or event callbacks. Invalid files preserve the live board.

Saved state includes maps, assignments, sheets, images by managed filename, combat,
defaults, instance properties, links and scripts. Entry authority, role, workspace,
edit lock and event callbacks stay in the running session. Pending forced moves
and trigger diagnostics clear on load. The UI writes a temporary file before
replacing Session.json. Back up managed Images alongside it; this local save is
not a portable asset package or a campaign transaction/approval ledger.

## Restricted script runtime

`FTabletopScriptRuntime` runs bundled Lua source in a fresh bounded state with a
host-created context. Scripts see immutable map/token snapshots appropriate to
their caller, and return proposed attribute/condition commands and log messages.
They have no filesystem, network, shell, package loader, native pointer or Unreal
object API. Memory and instruction budgets also bound otherwise valid Lua loops.

`TabletopScriptBridge.cpp` is the authority adapter. It supplies visible records,
limits player writes to permitted properties of owned tokens, validates every
returned command on a candidate board, and commits the resulting tokens together.
Failure applies none of that script's proposed mutations. Source parsing/runtime
success is not an authorization decision. Reentrant script effects are suppressed.

Manual macros use the invoking role, including shared DM-authored player macros.
DM-authored automatic events run during Game Play only. In a DM-entry local
session, the bridge supplies DM host context even during player preview and
keeps diagnostics out of player view. A standalone Player entry does not have
an authenticated host and cannot run DM event automations. A future network host
must construct the context from authenticated identity and filtered campaign
state; receiving a claimed role or owner ID is insufficient.

The restricted runtime is an initial implementation, not a claim that arbitrary
imported campaign code has a production security audit. See
[the Lua contract](Tabletop-Scripting.md) for functions, limits and source/license.

## Repository and assets

The Unreal host discovers `Plugins` through `AdditionalPluginDirectories`. Product
source is compiled in place; no junction, mirrored folder, or bidirectional sync
is necessary. The host descriptor is machine-local for now, and a script makes
the relationship reproducible in another project.

Reusable assets should live inside a tracked product plugin. The Lua implementation
is vendored source with its upstream license, rather than an engine-generated file
or a downloaded binary. Generated binaries,
caches, logs, and private campaign saves are excluded. Before significant asset
work, choose and verify versioned binary storage. Before release, track a minimal
host descriptor, target files, configuration, and required maps so a fresh checkout
can produce a complete packaged game without this temporary template.
