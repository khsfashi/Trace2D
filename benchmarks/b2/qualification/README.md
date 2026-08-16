# B2 baseline qualification fixtures

These files are **not scored B2 tasks**. They exist only to qualify candidate
Godot Agent bridges before the strongest baseline is selected and frozen.

`gameplay_loop_fixture` is intentionally smaller and semantically distinct from
the frozen top-down combat task. It proves a generic coding-agent feedback loop:
ordinary project files, launch, semantic input, structured runtime observation,
presentation capture, bounded candidate verification, clean teardown, and a
Trace2D-owned independent verifier that accepts the known-good fixture and
rejects a generated known-bad variant.

The scored prompt under `benchmarks/b2/tasks/` must never be supplied to this
qualification driver. Qualification evidence is non-scored and cannot authorize
B2 scoring by itself; the selected candidate identity and scored schedule are
frozen in a later P1 step.
