# Maintainer author style profile

Last reviewed: **2026-08-10**.

This file is a compact, reviewable style profile for the optional on-demand authoring layer defined by [`DEVELOPMENT_CONTENT_PIPELINE.md`](DEVELOPMENT_CONTENT_PIPELINE.md).

It is **not** Trace2D engineering authority. Technical facts come from Fact Packs, current repository evidence and current cited research. This profile only guides how an explicitly requested draft is explained and structured.

Approved public reference corpus:

- https://woodroot.tistory.com/

The corpus may evolve over time. Refresh this profile when newer maintainer-authored writing materially changes the pattern.

## Default Korean technical voice

Prefer:

- direct declarative Korean rather than corporate or promotional language,
- a concrete problem, strange observation, practical failure or question near the opening,
- the central proposition early enough that the reader knows what the article is investigating,
- sequential explanation: observation/problem -> cause -> structure/design -> consequences -> limitations -> implication,
- short-to-medium paragraphs with headings when the reasoning changes phase,
- definitions before deeper implications when introducing unfamiliar concepts,
- concrete game/client/engine examples where possible,
- explicit design tradeoffs and rejected alternatives,
- personal context only when it genuinely motivated the engineering problem,
- small lists, Q&A blocks or causal-flow diagrams when they reduce prose complexity,
- clear separation between what was observed, what is inferred, what was measured and what remains a hypothesis,
- a counterargument/limitations section for broad claims when useful,
- a compact final principle or practical takeaway instead of a marketing conclusion,
- references for research-heavy external claims.

Avoid by default:

- company-marketing voice,
- exaggerated adjectives such as revolutionary/groundbreaking without evidence,
- generic AI buzzword padding,
- pretending every design decision was obvious from the beginning,
- hiding failed ideas, uncertainty or `Not-tested` boundaries,
- invented autobiographical anecdotes,
- fabricated emotions or motives,
- mechanically copying memorable sentences from older posts,
- forcing every post into the same outline.

## Article modes

The same author has more than one useful mode. The Editorial Brief should choose rather than blending them mechanically.

### Engineering-thesis mode

Good for architecture, AI engineering and benchmark-methodology essays.

Typical shape:

```text
concrete observation / tension
 -> central question
 -> define the concept
 -> explain why the current structure exists
 -> propose or name the design idea when useful
 -> implications / practical criteria
 -> counterarguments and limits
 -> measurable or falsifiable interpretation when possible
 -> short conclusion
```

Use quotations/callouts sparingly for the core proposition or a particularly important boundary.

### Practical technical-explanation mode

Good for C++, renderer, engine internals and problem/bug explanations.

Typical shape:

```text
what the concept/problem is
 -> smallest concrete example
 -> why the observed behavior happens
 -> memory/runtime/performance consequences
 -> practical use / implementation notes
 -> caveats or related questions
```

Prefer code/examples over abstraction when they explain the mechanism more clearly.

### Development-log mode

Good for Trace2D subsystem work.

Preferred shape is not a PR summary. Instead:

```text
what problem appeared
 -> options considered
 -> why one choice was selected
 -> implementation consequences
 -> what was actually verified
 -> what remains uncertain / next
```

A development log may combine several PRs when they form one coherent engineering story.

## Evidence-aware writing

Style must never strengthen a claim beyond evidence.

Examples:

```text
implemented != tested
hosted CI != real-GPU evidence
benchmark design != benchmark result
one successful run != comparative superiority
Agent self-report != independent verification
```

If an important limitation materially changes the reader's interpretation of a claim, include it even if the requested angle is positive.

## Updating this profile

The maintainer's actual newer writing outranks this derived summary.

When refreshing:

1. sample several representative maintainer-authored posts rather than one outlier,
2. extract structural tendencies rather than copying phrases,
3. keep distinct article modes when the corpus shows them,
4. remove tendencies that no longer appear,
5. preserve the no-fabricated-biography and evidence-authority boundaries.
