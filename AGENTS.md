# Development guidance

## Product and authority
- Read Docs/Product-Decisions.md and Docs/Milestone-1.md before changing scope.
- Treat this checkout as canonical for reusable product code and design.
- Keep the local Unreal host a test harness; do not edit generated engine files.
- Keep campaign logic independent of 2D, 3D, and VR presentation.
- GPT-focused development means assisted engineering. An in-game AI service,
  API account, data upload, or paid dependency is not an accepted requirement.

## Autonomous work
- Routine reversible edits, builds, tests, local commits, development-branch
  pushes, and pull requests are authorized by the owner.
- Use development branches. Do not merge to main, publish releases, make
  purchases, change repository visibility, or destroy existing work without
  explicit authorization.
- Respect tool/sandbox permissions even when the work is authorized.
- Record important assumptions and unresolved product decisions in Docs.
- Do not commit personal campaign data, secrets, generated build output, or
  third-party assets without appropriate permission.

## Validation
- For C++ changes, build the connected host and run relevant Unreal automation
  tests. Report actual results and any skipped verification.
- Review offline submissions as untrusted proposals. Authenticate their author
  outside the domain model, enforce current permissions, and never accept a
  player's claim that an edit is minor or already approved.
- Do not move or copy existing Unreal assets merely to tidy the repository.
- Keep tooling paths configurable and document a reproducible setup.
