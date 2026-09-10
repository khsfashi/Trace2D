# Agent Skills and Progressive Discovery

Trace2D Agent Skills are small workflow contracts between capability truth and exact API/tool discovery:

```text
Capability: can Trace2D do this?
 -> Skill: what ordering/authority rules make the workflow correct?
 -> API/tool discovery: which exact public symbol or primitive is needed now?
```

## Entry points

Installed SDKs publish:

```text
<Trace2D root>/trace2d.agent-index.json
<Trace2D root>/share/Trace2D/agent/skills/registry-v1.json
<Trace2D root>/share/Trace2D/tools/trace2d_agent_skill.py
```

`find_package(Trace2D CONFIG REQUIRED)` exposes `Trace2D_AGENT_SKILL_REGISTRY`, `Trace2D_AGENT_SKILL_TOOL`, and `Trace2D_AGENT_SKILL_GUIDE`.

The registry contains only bounded descriptors. Load at most the task-relevant skill body; do not preload every `SKILL.md`.

## CLI

The packaged standard-library-only helper supports deterministic discovery:

```text
python trace2d_agent_skill.py list
python trace2d_agent_skill.py search "move player while key is held"
python trace2d_agent_skill.py get held-input-movement
python trace2d_agent_skill.py validate
```

Use `--registry <path>` when the registry is not beside the packaged tool/repository defaults. `list` and `search` return descriptors only. `get` is the only command that reads a body.

## V1 skills

- `held-input-movement` — semantic held/axis input sampled every fixed frame; never OS key-repeat movement.
- `semantic-menu-flow` — retained semantic activation consumed exactly once into canonical game state.
- `combat-hit-feedback` — gameplay hit truth first, audio/presentation feedback second.

## Design boundaries

A Skill is not a macro-tool and is not a second engine state model. It should contain workflow ordering, authority boundaries, important DON'T rules, and a verification recipe that are expensive to reconstruct from atomic APIs. Exact signatures belong to the public API index/headers; live callable primitives belong to MCP `tools/list`.

V1 deliberately does not add `trace2d.skill.*` MCP recipe tools. Trace2D's current MCP surface is already a small orthogonal primitive set. A future list/get transport endpoint is justified only if retained dogfood evidence shows that locating the packaged registry/tool remains a material cost.

No skill discovery, parsing, filesystem scan, JSON construction, or LLM-oriented work occurs in simulation/render/audio hot paths.
