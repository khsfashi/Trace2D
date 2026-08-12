# B1 task — deterministic animation with an exact semantic event

Work only inside the supplied workspace and use the lane's normal public engine/project workflow. Do not add a benchmark-only answer API or hard-code verifier output.

Author the existing three-frame animation with these exact frame durations and event semantics:

- frame 0: 100 ms,
- frame 1: 150 ms,
- frame 2: 250 ms,
- total duration: 500 ms,
- loop mode: loop,
- semantic event id/method meaning: `7` / the supplied redraw event,
- event time: exactly 250 ms from clip start,
- authored event ordinal: 0 where the lane exposes ordinals.

At exactly 250 ms, deterministic/headless inspection must report frame index 2 and the event must sit on that exact boundary. Keep structural verification separate from any screenshot/presentation capture. A presentation artifact at the same logical animation time is requested for later advisory review but must not replace exact verification.
