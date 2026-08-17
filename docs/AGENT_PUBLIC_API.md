# Agent Public C++ API Discovery

Trace2D agents and external-consumer tooling must discover public C++ APIs from repository-owned evidence instead of guessing header names from type names.

The machine-readable index is [`agent-public-api-v1.json`](agent-public-api-v1.json). It maps supported public symbols to the include path that actually declares them and to a compiling canonical external-consumer example.

## Discovery rule

For a public type such as `trace2d::application::Game`:

1. Query the machine-readable index for the fully qualified symbol.
2. Use its `include` field verbatim.
3. Inspect the referenced canonical example for normal construction/lifecycle usage.
4. If a symbol is not indexed, inspect the public include tree and current examples/tests before assuming support.
5. **Never manufacture a per-type header from the C++ type name.** Absence of `Game.hpp`, `SceneTemplate.hpp`, or a similar guessed file is not evidence that the engine is missing the type.

For the current application boundary, the canonical declaration is:

```cpp
#include <trace2d/application/Application.hpp>

class MyGame final : public trace2d::application::Game
{
    // ...
};
```

The canonical external-consumer implementation is under `examples/e0_external_game/`; it is ordinary public API usage, not benchmark-only scaffolding.

## Scope

This index is intentionally small and evidence-backed. Add a symbol only when its public contract and canonical usage are stable enough to help agents avoid repository search/guessing. It is a discovery aid, not a generated reflection database, ABI promise, or substitute for compiling code and tests.
