# Live project state and deterministic handoff

Trace2D develops quickly enough that committed Markdown cannot be the authority for volatile GitHub facts such as whether a PR is still draft, whether it merged seconds ago, or which CI run is current.

The repository therefore separates two classes of truth:

```text
durable committed lane/order/gates
        +
live GitHub issue / PR state
        ↓
scripts/project_state.py
        ↓
versioned operational next action
```

## Command

From the repository root:

```bash
python scripts/project_state.py --json
```

The resolver reads `config/trace2d.core-lane.json`, then queries the public GitHub API. `GITHUB_TOKEN` or `GH_TOKEN` is used when available; public unauthenticated reads are permitted as a fallback.

The output schema is `trace2d.project-state.v1`.

Important fields include:

- `live.available`: whether live GitHub state was actually available,
- `owner_detours`: explicit owner-directed hardening/governance work that temporarily precedes the fixed core lane,
- `core.current`: the current detour/core stage derived from live issue/PR state,
- `core.previous_core_stage`: the last completed Sprite stage before the current one,
- `next_action`: the exact operational action (`continue_pull_request`, `implement_issue`, `create_issue`, or `inspect_live_github`).

## Offline / unavailable GitHub

The resolver must never convert stale Markdown into fake live truth.

If GitHub cannot be reached, it emits:

```json
{
  "live": {"available": false},
  "next_action": {"kind": "inspect_live_github"},
  "blockers": ["live_github_unavailable"]
}
```

and exits non-zero. A coding agent must stop live-state inference at that point rather than guessing which PR merged.

## Deterministic fixtures

`--snapshot <path>` replaces network access with a committed GitHub-state fixture. This is the CI contract surface and is intentionally deterministic.

Fixtures cover:

- an active draft owner detour,
- completion of both owner hardening detours followed by SPP2 becoming the next ready stage,
- the second SA2 hardening detour becoming current after the first completes,
- explicit GitHub-unavailable behavior through the resolver API.

The project-state workflow also runs the same snapshot twice and byte-compares JSON output.

## Authority boundary

`PROJECT_STATUS.md` remains useful human-readable roadmap and handoff context, but PR draft/merge/CI/branch facts are volatile. They are not made true merely because Markdown says so.

Routine `Trace2D next` continuation should use this order:

1. read the durable Agent/product contracts,
2. run/read live project state,
3. inspect the exact live issue/PR/checks selected by that state,
4. use `PROJECT_STATUS.md` for explanatory context and reconcile stale prose when useful,
5. never auto-commit GitHub state back into Git simply to mirror a merge.

The fixed lane is committed; live state is derived.
