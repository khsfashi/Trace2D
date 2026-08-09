# MCP Transport

Trace2D exposes its protocol-independent automation surface through a narrow Model Context Protocol (MCP) adapter.

The authoritative engine contracts remain:

```text
Runtime / Scene / Input / UI
          |
          v
      AgentFacade
          |
          v
  GameplayScenario
          |
          v
      engine/mcp
          |
          v
   tools/trace2d_mcp
```

MCP is transport. It does not own gameplay state, UI state, simulation time, selectors, assertions, renderer state, or input semantics.

## Protocol and transport

The primary wire version is MCP `2026-07-28`.

Trace2D follows the final protocol shape relevant to this adapter:

- JSON-RPC 2.0 messages,
- `server/discover`,
- per-request `params._meta` carrying `io.modelcontextprotocol/protocolVersion` and `io.modelcontextprotocol/clientCapabilities`,
- `resultType = "complete"` on modern successful results,
- server identity in result `_meta`,
- deterministic `tools/list`,
- `ttlMs` / `cacheScope` cache hints,
- `tools/call`,
- newline-delimited UTF-8 stdio transport.

The host also accepts the older `initialize` request shape as a narrow compatibility probe for clients that fall back from `server/discover`. The modern Trace2D MCP contract is the `2026-07-28` path above.

Official MCP references:

- https://modelcontextprotocol.io/specification/2026-07-28
- https://github.com/modelcontextprotocol/modelcontextprotocol/tree/main/schema/2026-07-28

## Dependency boundary

`nlohmann/json` is private to `Trace2D::Mcp` and MCP protocol tests.

The following modules remain JSON/MCP-free:

```text
engine/core
engine/runtime
engine/scene
engine/input
engine/ui
engine/agent public contracts
engine/testing public contracts
```

The adapter may allocate strings, JSON objects, and copied inspection snapshots only when an MCP request explicitly asks for them. Normal simulation, input advancement, UI state mutation/rasterization, and rendering do not construct MCP messages.

## Host

Build the repository with the normal preset, then launch:

```powershell
out/build/windows-msvc/tools/mcp/Debug/trace2d_mcp.exe `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --ui samples/ui/basic_ui.trace2d.toml `
  --seed 42
```

The process reads exactly one JSON-RPC message per stdin line and writes exactly one MCP response per stdout line. Diagnostics go to stderr so stdout remains protocol-clean.

No renderer, window, GPU device, SDL video subsystem, or screen capture is initialized by this host.

## Required modern request metadata

Every `2026-07-28` request uses the following metadata shape inside `params`:

```json
"_meta": {
  "io.modelcontextprotocol/protocolVersion": "2026-07-28",
  "io.modelcontextprotocol/clientInfo": {
    "name": "example-client",
    "version": "1.0"
  },
  "io.modelcontextprotocol/clientCapabilities": {}
}
```

`clientInfo` is optional in the final protocol. Trace2D does not use client identity to change simulation behavior.

## Tools

The tool list is emitted in fixed order and is cacheable for 60 seconds with public cache scope because tool availability does not depend on user/session state.

| Tool | Contract reused |
| --- | --- |
| `trace2d.inspect` | `AgentFacade::Inspect` |
| `trace2d.query` | `AgentFacade::Query` / `QueryOne` |
| `trace2d.ui.inspect` | `AgentFacade::InspectUi` |
| `trace2d.ui.query` | `AgentFacade::QueryUi` / `QueryOneUi` |
| `trace2d.ui.focus` | `AgentFacade::FocusUi` |
| `trace2d.ui.activate` | `AgentFacade::ActivateUi` |
| `trace2d.ui.input_text` | `AgentFacade::InputUiText` |
| `trace2d.ui.assert` | `AgentFacade::AssertUi` |
| `trace2d.input.schedule` | `GameplayScenario` / `InputSystem::Schedule` |
| `trace2d.input.inspect` | `InputSystem::State` |
| `trace2d.runtime.step` | `GameplayScenario::RunFrames` |
| `trace2d.assert_float` | `GameplayScenario::AssertFloatFieldEquals` |

`trace2d.runtime.step` accepts `1..100000` frames per call. This is a transport-side work bound; it does not change runtime stepping semantics.

## End-to-end stdio workflow

The examples below are single lines. They are formatted across lines here only for readability; stdio messages themselves must not contain embedded newlines.

### 1. Discover the server

```json
{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientInfo":{"name":"example-client","version":"1.0"},"io.modelcontextprotocol/clientCapabilities":{}}}}
```

The response advertises the supported version, the tools capability, stable cache hints, and Trace2D server identity in result `_meta`.

### 2. Inspect deterministic runtime/scene state

```json
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"trace2d.inspect","arguments":{},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

Runtime frame, seed, fixed-step duration, simulation time, scene identity, entity identities, transforms, and typed component fields come from the existing `AgentFacade` snapshot.

### 3. Query `#player`

```json
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"trace2d.query","arguments":{"selector":"#player","one":true},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

The selector is the same semantic selector used by the CLI and tests. MCP does not introduce a second selector language.

### 4. Query and activate semantic UI

The committed UI sample contains the semantic button `role=button`, `name="Start Game"`.

```json
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"trace2d.ui.query","arguments":{"selector":{"role":"button","name":"Start Game"},"one":true},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

```json
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"trace2d.ui.activate","arguments":{"selector":{"role":"button","name":"Start Game"}},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

No screen coordinate is required. Bounds remain observable diagnostic data only.

### 5. Schedule deterministic input and step

```json
{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"trace2d.input.schedule","arguments":{"frame":2,"control":"key_d","event":"press"},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

```json
{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"trace2d.input.schedule","arguments":{"frame":6,"control":"key_d","event":"release"},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

```json
{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"trace2d.runtime.step","arguments":{"frames":8},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}
```

The standalone generic host advances the loaded scenario and its input state. An embedding game/tool may provide the existing `GameplayFrameUpdate` callback to connect those frames to game-specific deterministic update logic. The MCP protocol test does exactly that and proves scheduled `KeyD` input moves `#player`, after which the existing semantic query and gameplay assertion verify the result.

### 6. Structured failures

Gameplay and UI assertion failures are returned as normal MCP tool results with `isError = true` and machine-readable `structuredContent`.

Gameplay failures preserve existing reproducibility fields including:

- failure code,
- semantic selector,
- component and field,
- expected and observed typed values,
- exact frame,
- deterministic seed,
- runtime/input/entity snapshot context.

UI failures preserve the semantic selector, stable Agent error code/message, and observed UI state when available.

Transport/schema failures such as an unknown tool name remain JSON-RPC errors rather than gameplay/UI errors.

## Performance policy

MCP is intentionally not a frame-loop service layer.

- no JSON work runs unless a message arrives,
- no MCP snapshot is retained between calls,
- `tools/list` is deterministic/cacheable,
- virtual input scheduling is setup work and reuses the existing input queue,
- explicit stepping uses the existing fixed-step scenario path,
- one request is bounded to 100,000 frames,
- the host initializes no renderer/GPU resources,
- the adapter does not add indexes/caches to engine semantic state until measurement justifies them.

This keeps the cost of LLM observability opt-in and outside ordinary gameplay hot paths.
