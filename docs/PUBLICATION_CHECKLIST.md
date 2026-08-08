# Public Alpha Publication Checklist

Use this checklist only after the release-ready status PR is merged and the resulting latest `main` CI is green.

## GitHub Release

- Tag: `v0.1.0-alpha.1`
- Target: `main`
- Release title: `Trace2D v0.1.0-alpha.1 — Public Alpha`
- Release notes source: `docs/RELEASE_NOTES_v0.1.0-alpha.1.md`
- Pre-release: enabled
- Binary attachments: none for this source-first alpha unless the exact resolved runtime dependency graph and notices are separately reviewed

## Visibility

After the release exists, change repository visibility from Private to Public.

## Public verification

Verify from an unauthenticated/incognito browser:

- repository root opens without authentication
- README renders and internal links work
- root `LICENSE` shows MIT
- `docs/THIRD_PARTY.md` is reachable
- `docs/PUBLIC_ALPHA_LIMITATIONS.md` is reachable
- `docs/PUBLIC_ALPHA_SAMPLE.md` is reachable
- release `v0.1.0-alpha.1` is visible and marked pre-release
- clone URL works without authentication

## Completion

After public verification:

- close Issue #14 as completed
- update `PROJECT_STATUS.md` in the first post-release change to record that publication/public-view verification is complete
- open only concrete post-alpha issues justified by known limitations or measured needs
