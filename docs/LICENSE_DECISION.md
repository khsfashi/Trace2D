# Project License Decision

Trace2D must have an explicit repository license before changing visibility to Public. The project was intentionally created without a license so this decision would be made deliberately at the release gate.

The two current candidates are MIT and Apache License 2.0.

## MIT

Advantages:

- very short and familiar,
- permissive commercial/private/modified use,
- minimal contributor and consumer friction,
- common for small libraries, tools, samples, and portfolio projects.

Tradeoff:

- does not contain Apache-2.0's explicit patent-license language.

## Apache License 2.0

Advantages:

- permissive commercial/private/modified use,
- explicit patent grant and patent-termination terms,
- explicit rules around notices and modified files,
- often attractive when a project may later receive broader corporate contributions.

Tradeoff:

- longer and somewhat more administratively explicit than MIT.

## Dependency compatibility

The direct Public Alpha dependency set reviewed in `THIRD_PARTY.md` is permissively licensed (zlib, MIT, BSD-3-Clause). Either candidate is suitable for Trace2D's intended source release.

## Recommendation criteria

Choose **MIT** when the main priority is the simplest possible permissive license and lowest reading/administrative overhead.

Choose **Apache-2.0** when the main priority is retaining permissive adoption while making the patent grant explicit for future external/corporate use and contributions.

No license is selected by this document. Adding the root `LICENSE` file is intentionally blocked on the repository owner's explicit choice.

After the choice is made:

1. add the canonical root `LICENSE` text,
2. update README licensing text,
3. run `scripts/release_audit.ps1 -RequireLicense`,
4. mark the license gate complete in `PROJECT_STATUS.md`, `docs/PUBLIC_RELEASE.md`, and Issue #14.
