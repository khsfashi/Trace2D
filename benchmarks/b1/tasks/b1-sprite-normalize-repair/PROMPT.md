# B1 task — normalize and repair a trimmed Sprite

Work only inside the supplied workspace and use the lane's normal public engine/project workflow. Do not add a task-specific verifier bypass or print a hard-coded success result.

The authored Sprite represents one 16x16 source frame whose visible trimmed content begins at source pixel (2, 1) and is 12x14 pixels. Normalize the existing asset/resource so the visible region matches that geometry, uses nearest sampling, and aligns to the exact source-space pivot (8, 8).

Requirements:

- source size: 16x16,
- trim offset: (2, 1),
- trim size: 12x14,
- source-space pivot/alignment anchor: (8, 8),
- nearest sampling,
- preserve the existing semantic Sprite identity,
- do not infer a different crop from screenshots.

Produce deterministic/headless-verifiable content first. Presentation evidence may be captured separately and is advisory, not deterministic truth.
