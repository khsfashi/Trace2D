# Sprite Generation Orchestration — SPP5

Status: **active via #168**  
Parent: #59 Complete Sprite  
Predecessor: SPP4 / #166 / PR #167 / squash `e195afb2a9dc7c80f49d71abff32c920e3e850c4`

## 1. Goal

SPP5 adds the first provider-neutral generation orchestration seam to Trace2D without making a model, SDK, network protocol or provider response part of engine runtime truth.

The authority split is explicit:

```text
provider-neutral request + deterministic post-process plan
 -> replaceable external SpriteGenerationProvider
 -> nondeterministic owned candidate response
 -> deterministic candidate validation
 -> existing SPP2 quality/repair and/or SPP4 manifest validation
 -> existing SPP3 import
 -> existing S1 canonical validation
 -> canonical SpriteAsset only after every required gate passes
```

A live provider call is never a deterministic CI dependency. CI uses fake/recorded provider responses and proves the deterministic post-generation half of the pipeline.

## 2. Provider boundary

`SpriteGenerationProvider` is a protocol-independent C++ interface with one explicit `Generate(request)` call. Trace2D does not ship a provider SDK, HTTP client, API key surface, background worker or model-specific request schema in SPP5.

The provider receives only a provider-neutral request:

- stable request ID,
- human/agent prompt text,
- exact expected frame count.

A successful provider response must echo the request ID and provide non-empty provider ID/revision evidence. Those strings identify the external input source; they are not canonical Sprite data.

Provider execution may be nondeterministic. Re-running a live provider is not expected to reproduce pixels. The deterministic guarantee begins once one concrete owned provider response is supplied to the Trace2D post-generation pipeline.

## 3. Candidate kinds

SPP5 supports two explicit finite candidate shapes.

### 3.1 Provider-neutral loose frames

The caller supplies ordered canonical targets before provider execution:

- page ID,
- region ID,
- texture reference,
- optional exact rational pivot.

The provider supplies only ordered owned RGBA8 pixel buffers and dimensions. It does not author canonical page/region/texture identity.

The path is:

```text
owned generated RGBA8 frames
 -> exact frame-count/dimension/byte validation
 -> SPP2 AnalyzeAndRepairSpriteQuality
 -> caller-selected bounded deterministic repair, if any
 -> SPP3 ImportLooseSpriteFrames
 -> S1 canonical serializer/parser validation
```

SPP2 findings remain policy-labelled evidence. An SPP2 failure or an SPP3/S1 failure prevents canonical output from being exposed by the SPP5 result.

### 3.2 Generator manifest atlas

A provider adapter may instead return one owned atlas plus an explicit SPP4 manifest kind and manifest text.

The path is:

```text
owned atlas + explicit manifest kind/text
 -> exact atlas RGBA8 validation
 -> SPP4 ImportSpriteGeneratorManifestJson
 -> SPP3 generic sheet lowering
 -> S1 canonical validation
 -> exact expected-frame-count gate
```

SPP5 does not auto-detect manifest formats. SPP4 remains the only finite manifest adapter authority. A post-import expected-count mismatch discards the successful intermediate import from the returned authoritative surface.

## 4. Transactionality

Preflight validation runs before provider execution. Invalid request/plan state therefore causes zero provider calls.

Examples of preflight rejection:

- empty request ID or prompt,
- zero expected frame count,
- loose target count different from expected frame count,
- missing canonical loose-frame IDs/references,
- invalid explicit/default pivot denominator,
- manifest mode mixed with loose-frame targets,
- missing manifest canonical asset/page/texture identity.

After provider execution, any of the following prevents canonical output:

- provider failure/exception,
- response request/provider identity failure,
- candidate-kind mismatch,
- malformed RGBA8 shape,
- expected frame-count mismatch,
- SPP2 failure,
- SPP3/S1 import failure,
- SPP4 manifest failure.

No partially accepted canonical SpriteAsset becomes authoritative through `SpriteGenerationResult::Succeeded()`.

## 5. Deterministic evidence

`SerializeSpriteGenerationResultJson(...)` emits `trace2d.sprite-generation-result.v1` structural evidence containing:

- request/provider/revision identity,
- explicit candidate kind,
- whether provider execution occurred,
- validated candidate frame count,
- success state,
- nested deterministic SPP2 result when the loose-frame path is used,
- nested deterministic SPP3/S1 import result when available,
- nested deterministic SPP4 result when the manifest path is used,
- ordered typed SPP5 diagnostics.

The same recorded response + request + plan must produce byte-identical SPP5 JSON. Different live provider runs are not required to produce the same bytes.

## 6. Performance and ownership

SPP5 is explicit offline/setup work only.

- preflight and envelope checks are `O(frame target count)`,
- loose candidate shape validation is `O(frame count)` plus exact byte-size arithmetic,
- loose pixel work reuses SPP2 costs and then SPP3 `O(frame count)` import,
- manifest work reuses SPP4 `O(manifest bytes + frames)` planning plus SPP3/S1 validation,
- provider response pixels are owned once by the provider response and viewed by downstream analyzers/importers where existing APIs permit,
- repaired RGBA8 copies occur only when the caller explicitly requests SPP2 repair,
- no generation/provider/QA/JSON/filesystem/network work enters fixed-step animation or normal rendering,
- no new package/dependency is introduced.

Optimization complexity should remain bounded by measured offline generation workloads; SPP5 does not add speculative caching, worker pools or provider retry policy.

## 7. CI and live-provider policy

Portable CI correctness uses fake/recorded providers only.

Required tests cover:

- one provider call on valid orchestration,
- zero provider calls on invalid preflight,
- provider exception/failure transactionality,
- exact loose expected-frame gates,
- SPP2 -> SPP3/S1 success,
- SPP4 -> SPP3/S1 manifest success,
- manifest post-import expected-count rejection without canonical exposure,
- byte-identical structural JSON for the same recorded response.

Live/paid provider smoke tests may be added later as explicit opt-in evidence, but they cannot gate deterministic repository correctness and must not require secrets for normal contributors.

No real-GPU gate is required because SPP5 changes no presentation behavior.

## 8. Non-goals

SPP5 does not add:

- OpenAI/Stability/other provider SDKs,
- HTTP/auth/secrets management,
- provider retry/backoff/rate-limit policy,
- background generation workers,
- prompt optimization or model routing,
- learned/VLM quality judgment as deterministic truth,
- atlas packing,
- runtime generation,
- automatic semantic naming/pivot inference,
- canonical asset mutation after import,
- multimodal final-art approval.

Perceptual review remains the existing multimodal/human layer. SE2E will prove the complete generated/imported Sprite workflow on the next Sprite child.

## 9. Handoff

When #168 is green and merged, SPP0-SPP5 is complete. Stop this continuation. The exact next #59 child is **SE2E**, created only by the following continuation.
