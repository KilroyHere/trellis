# ADR-0013: Governance and release discipline

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-13; §10 (release cadence), §15; R12, R17; risk K7
- **Spikes:** none.

## Context

This is the largest risk the research identified (R12): every existing framework depends on one maintainer, and fixes sit untagged (Boost.DI's 2026 compiler fixes; Fruit's 26 commits unreleased since January 2023). Fruit's Windows CI fails and its macOS CI is cancelled every month without anyone noticing. It is recorded as an ADR because it shapes the project as much as any technical decision.

## Decision

As plan §15 sets out:

- **Maintainers:** two with merge rights before v0.5, plus CODEOWNERS, branch protection and required reviews, and a short `MAINTAINERS.md` with a release and triage rota.
- **Release discipline:** a patch release within 7 days of any merged fix, no fix untagged for more than 14 days, and a minor release every 6–8 weeks during 0.x (§10). Package recipes update automatically.
- **CI hygiene:** a nightly full matrix; failures open issues and notify both maintainers; a red nightly blocks releases.
- **Triage:** a first response within 7 days, labels, and a monthly review of open issues.
- **Simplicity budget:** core headers stay at or under 6,000 lines (NFR-12), and every metaprogramming trick gets a comment, a test and an entry in `docs/internals/`.
- **Compatibility promise:** the list of supported compilers changes only in minor releases, with one release of notice; the C++20 baseline holds for all of 1.x; public API changes after 1.0 follow semantic versioning, and removals are deprecated for at least one minor release first (R17).

## Alternatives

- **One maintainer with best-effort releases:** the failure mode the research found in every framework.
- **Co-maintaining an existing framework instead of building one** (plan §1, option 0): fixes the bus factor directly, but gives up this plan's design goals.

## Consequences

- **What exists so far** (docs/PROGRESS.md): CI runs on every push for six Linux presets (T0.2); a nightly workflow runs TSan and the quick compile-time benchmark; a failing scheduled workflow opens or updates a single issue. The license is the Boost Software License 1.0 (D3), and the copyright holder is Aryan Patel (T0.11).
- **macOS and Windows run weekly or on demand,** because the repository is private and Actions minutes are limited. That slows feedback on the MSVC risks in ADR-0003, ADR-0006 and ADR-0014, which only CI can test. Making the repository public would allow the full matrix on every push; that is the owner's call.
- The second maintainer is needed before v0.5 (risk K7).

## History

- **2026-10-30, plan draft 1:** drafted as ADR-13 in plan §4, pointing to §15.
- **2026-10-31, T0.8:** recorded in this file, unchanged; the current CI arrangement added to the consequences.
