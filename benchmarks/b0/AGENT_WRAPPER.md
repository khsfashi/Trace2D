# B0 frozen Agent wrapper contract

The benchmark harness does **not** hard-code one provider SDK or one coding-agent product. Instead, one external wrapper is frozen for a benchmark cohort and described by `agent-profile.json`. The exact same profile file must be used for every lane of the same task; reports reject mixed profile hashes.

## Inputs

The harness invokes the profile `command` inside a fresh isolated task workspace. Command placeholders are expanded before launch:

- `{workspace}` — fresh candidate workspace,
- `{prompt}` — copied common task prompt,
- `{lane}` — `godot.generic`, `godot.agent`, or `trace2d.agent`,
- `{task}` — task ID,
- `{agent_result}` — path where the wrapper must write its structured result.

It also exports:

```text
TRACE2D_BENCH_LANE
TRACE2D_BENCH_TASK
TRACE2D_BENCH_WORKSPACE
TRACE2D_BENCH_PROMPT_FILE
TRACE2D_BENCH_AGENT_RESULT
TRACE2D_BENCH_MAX_TOOL_CALLS
TRACE2D_BENCH_MAX_INPUT_TOKENS
TRACE2D_BENCH_MAX_OUTPUT_TOKENS
```

The lane changes the available engine/adapter configuration, **not** the Agent/model identity, prompt intent, or budget.

## Required result

The wrapper writes UTF-8 JSON to `TRACE2D_BENCH_AGENT_RESULT`:

```json
{
  "schema_version": 1,
  "status": "completed",
  "model": {
    "agent_id": "same-as-profile",
    "model_id": "same-as-profile",
    "model_revision": "same-as-profile"
  },
  "human_interventions": 0,
  "metrics": {
    "revisions": 2,
    "tool_calls": 19,
    "input_tokens": 12000,
    "output_tokens": 1800,
    "normalized_operations": {
      "file_read": 6,
      "file_write": 2,
      "runtime_inspect": 3,
      "runtime_input": 0,
      "verify": 2
    },
    "engine_native_operations": {
      "example_native_tool_name": 3
    }
  }
}
```

`status` may be `completed` or `tool_transport_failure`. Other wrapper/process failures are classified by the harness rather than silently becoming implementation failures.

## Metric boundary

Provider/model token counts must come from the provider/client's own usage accounting when available. Do not estimate token usage by re-tokenizing text with a different tokenizer.

`normalized_operations` and `engine_native_operations` are intentionally separate:

- normalized operations make broad workflow cost comparable,
- engine-native operations preserve the actual tool shape and prevent the normalization layer from erasing architectural differences.

The benchmark publishes both. There is no composite score that trades correctness against token count or wall time.

## Generic Godot lane

`godot.generic` means the Agent receives normal coding/filesystem/process capabilities and a stock Godot project, but no Godot-specific MCP/Agent bridge. It may still run Godot normally through shell/process tools if the frozen Agent wrapper ordinarily permits that.

## Godot Agent lane

`godot.agent` uses the exact bridge/version pinned by `suite.json` and its qualification evidence. The wrapper may expose that MCP server to the same coding Agent, but may not add task-specific tools, prompts, scripts, or hidden solution hints.

## Trace2D Agent lane

`trace2d.agent` exposes the normal public Trace2D CLI/MCP/Agent surface available at the frozen Trace2D source commit. Benchmark-only engine behavior is forbidden.
