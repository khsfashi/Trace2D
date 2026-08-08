# Project License Decision

Trace2D uses the **MIT License** for the project source beginning with the `v0.1.0-alpha.1` Public Alpha release.

The repository owner explicitly selected MIT on 2026-08-08 after reviewing MIT and Apache License 2.0 as the two prepared permissive-license candidates.

## Decision

Selected license: **MIT**

Why MIT fits the current project:

- short, familiar, and easy for contributors and portfolio reviewers to understand,
- permissive commercial, private, and modified use,
- minimal administrative overhead for a small source-first engine project,
- compatible with the direct Public Alpha dependency set reviewed in `THIRD_PARTY.md`.

The main tradeoff versus Apache-2.0 is that MIT does not contain Apache-2.0's explicit patent-license and patent-termination language. That tradeoff was accepted for the current project and release scope.

## Dependency compatibility

The direct Public Alpha dependency set reviewed in `THIRD_PARTY.md` is permissively licensed:

- SDL3 — zlib
- SDL3_shadercross — zlib
- toml++ — MIT
- GoogleTest — BSD-3-Clause, tests/development

The project-level MIT license does not replace or relicense third-party components. Their original license obligations remain separate.

## Release gate

The canonical MIT text is stored in the repository root as `LICENSE`.

Release-candidate validation must require that file:

```powershell
./scripts/release_audit.ps1 -RequireLicense
```

CI runs that strict form for the Public Alpha release gate. Future changes to the project license must be explicit repository-owner decisions and must update the root license, README, release documentation, and third-party review as applicable.
