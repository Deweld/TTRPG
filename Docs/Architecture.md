# Architecture

## Boundaries

`TTRPGCore` owns system-neutral campaign records and the change-review rules.
Later modules will provide persistence, authenticated session transport, tabletop
presentation, and tooling UI. Core records use stable IDs rather than actor
pointers so they can survive saves, offline copies, and alternative interfaces.

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

## Current code versus planned work

Implemented now: existing-record property edits, ownership/edit grants,
record-level revision checks, a DM field allowlist for minor changes, explicit
approval/rejection, atomic in-memory application, a before/after decision log,
and duplicate-ID rejection after a decision.

Not implemented yet: authenticated sessions, visibility-filtered export, a durable
pending queue, JSON serialization, disk transactions, typed sheet schemas,
record creation/deletion, macro execution, or UI. The domain tests exercise the
rules directly and are not evidence of a working multiplayer or offline UI.

## Repository and assets

The Unreal host discovers `Plugins` through `AdditionalPluginDirectories`. Product
source is compiled in place; no junction, mirrored folder, or bidirectional sync
is necessary. The host descriptor is machine-local for now, and a script makes
the relationship reproducible in another project.

Reusable assets should live inside a tracked product plugin. Generated binaries,
caches, logs, and private campaign saves are excluded. Before significant asset
work, choose and verify versioned binary storage. Before release, track a minimal
host descriptor, target files, configuration, and required maps so a fresh checkout
can produce a complete packaged game without this temporary template.
