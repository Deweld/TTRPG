# Milestone 1: first hosted tabletop session

## Acceptance scenario

Using two application instances on Windows:

1. A DM creates and hosts a campaign; a player joins it.
2. The DM and player receive appropriate tools. The player cannot edit someone
   else's character or inspect hidden DM data through the session payload.
3. The player opens a configurable sample sheet and rolls dice into a shared log.
4. The player moves an owned token on a top-down tabletop; the DM and player
   observe the same accepted location.
5. The player disconnects, edits an allowed sheet field locally, reconnects, and
   submits the draft against its original approved revision.
6. The DM sees before/after values and can approve or reject the submission.
   Explicitly configured minor edits can be accepted automatically; major edits
   and stale revisions remain visible for review/reconciliation.
7. The DM saves, closes, and reloads the campaign. Approved state and decision
   history persist; retrying a decided submission does not apply it twice.

## Implementation sequence

- [x] Record product decisions and repository/host boundary.
- [x] Create a reusable runtime plugin and repeatable host setup/build/test scripts.
- [x] Implement and test in-memory review rules for offline property edits.
- [ ] Add versioned save format, atomic persistence, and restoration tests.
- [ ] Add authenticated session commands and visibility-filtered player snapshots.
- [ ] Add host/join flow with two-instance integration coverage.
- [ ] Add sample sheet UI and shared dice log.
- [ ] Add top-down tabletop and authorized token movement.
- [ ] Add offline draft storage, submission queue, and DM review/filter UI.
- [ ] Verify the complete acceptance scenario and a fresh project setup.

The first three tasks establish the foundation; they do not constitute the
playable milestone. Windows mouse/keyboard and direct/LAN sessions are the initial
engineering defaults. Internet lobby integration and commercial entitlements
follow after the session workflow is proven.
