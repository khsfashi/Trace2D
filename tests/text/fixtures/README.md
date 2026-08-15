# Text test font fixtures

These files are intentionally tiny synthetic fonts used only by Trace2D text tests. They are repository test data, not production font assets, and do not depend on fonts installed on the CI host.

- `Trace2DTestFont.ttf` is the existing synthetic fixture covering space, `A`, Korean `한` (U+D55C), and CJK `中` (U+4E2D).
- `Trace2DPrimaryTestFont.ttf` is a deterministic derivative of that fixture for F3 fallback tests. Its Unicode cmap intentionally omits `한`, while retaining `A` and `中`. The `A` horizontal advance is changed from 600 to 500 font units so tests can prove that reversing an ordered fallback chain selects a different source font when both fonts cover the same codepoint.

The derivative changes only test metadata/metrics/coverage; it introduces no external font package or runtime dependency. Keep both files small and deterministic so text CI remains independent of OS font discovery.
