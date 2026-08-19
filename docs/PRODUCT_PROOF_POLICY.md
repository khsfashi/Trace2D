# Product Proof Policy

Trace2D does not treat subsystem completion alone as sufficient product evidence.

## Rule

After roughly two substantial new production subsystems, run a bounded external product proof using the existing public engine/SDK/Agent surfaces before continuing breadth.

The proof must answer a real use question, not merely reproduce unit-test coverage.

Current intended checkpoints:

```text
#89 Material2D / Shader2D
#90 Tween / Sequence
 -> Presentation Product Proof

#76 Physics2D
#77 Audio
 -> small combat / game-feel product proof

#91 Profiler
#78 Linux / non-MSVC
#92 tiered GPU QA
 -> cross-platform / stress product proof

#79 Save / Migration
 -> real exit / restart / migration product proof
```

## Boundaries

- Product proofs should primarily compose existing public capabilities, not create broad new engine architecture.
- A proof may expose a concrete missing primitive. Fix only the smallest demonstrated gap before repeating the proof.
- Preserve owner-rejected or failed proof evidence; do not rerun solely to erase an unfavorable result.
- Deterministic correctness, performance evidence and human-visible product quality are separate signals.
- A visually weak but technically correct proof is not automatically a product success.

## Relationship to TraceResearch

Trace2D owns making the engine useful and proving representative product workflows can be completed.

TraceResearch owns controlled comparative claims such as whether Trace2D reduces Agent context, tool calls or repair loops relative to another authoring surface.

Do not grow benchmark-specific infrastructure inside Trace2D merely to improve a research score.
