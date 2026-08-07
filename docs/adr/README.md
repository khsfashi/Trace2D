# Architecture Decision Records

ADRs preserve important project decisions so future contributors and coding agents do not need chat history to understand why the architecture looks the way it does.

## When to write an ADR

Create or amend an ADR when a change affects one of these:

- dependency direction between engine modules
- deterministic simulation guarantees
- authored scene/project format
- stable identity/handle model
- machine-facing agent contract
- automation transport/protocol choice
- rendering/platform backend strategy
- ownership/lifetime model with broad impact
- first-public-release scope when it materially changes

Normal implementation details do not need ADRs.

## Status values

Use one of:

- `Proposed`
- `Accepted`
- `Superseded by ADR-XXXX`
- `Rejected`

## Template

```markdown
# ADR-XXXX: Decision title

- Status: Proposed
- Date: YYYY-MM-DD

## Context

What problem or constraint requires a decision?

## Decision

What are we choosing?

## Consequences

What becomes easier, harder, required, or explicitly unsupported?

## Alternatives considered

What serious alternatives were considered and why were they not selected?
```

ADRs are not immutable. If a decision changes, preserve the old record and supersede it so the repository keeps the reasoning history.
