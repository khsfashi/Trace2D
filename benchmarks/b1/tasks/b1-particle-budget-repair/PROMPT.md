# B1 task — diagnose and repair a particle budget defect

The supplied particle content is intentionally over-budget. Diagnose the authored defect and repair it through the lane's normal public engine/project workflow. Do not add task-specific verifier logic or a benchmark-detection fast path.

Frozen deterministic constraints:

- authored particle capacity/amount must be at most 64,
- periodic/burst spawn attempts represented by the fixture must be at most 8 per simulation step/frame,
- maximum authored particle lifetime must be at most 6 Trace2D fixed steps (Godot matched fixture uses the frozen 0.8 s lifetime),
- the qualification/scored fixture must remain authored non-emitting at load time,
- preserve the existing effect identity and one-shot intent.

Fix the seeded defect while retaining a useful visible effect. Deterministic structural/performance-budget evidence is authoritative; presentation capture and multimodal quality review are separate advisory evidence.
