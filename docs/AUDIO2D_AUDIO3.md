# Audio2D AUDIO3 — deterministic voice admission and observable stealing

Issue: #365, third bounded implementation slice of #77.

## Purpose

AUDIO1 owns semantic playback and AUDIO2 owns decoder/preparation state. AUDIO3 fixes the simultaneous-voice admission policy before the SDL3 presentation backend is allowed to mirror those voices.

The key rule is:

> Physical output may mirror semantic voice decisions, but it must never invent a second hidden voice-limit or stealing authority.

## Default compatibility

`AudioSystem2D` still uses retained `ReserveVoices()` capacity as the storage ceiling. The new `AudioVoiceLimits2D` defaults to unlimited global/per-group semantic limits and `reject_new` overflow behavior.

Therefore existing callers that do not configure voice limits retain the AUDIO1 behavior: once retained voice capacity is full, `Play()` returns `capacity_exceeded`.

## Explicit limits

`SetVoiceLimits()` accepts:

- one global simultaneous-voice limit;
- one limit for each finite `AudioGroup2D` value;
- `reject_new` or `steal_oldest` overflow policy.

Lowering limits below the already-active semantic population fails closed. Configuration does not silently kill existing voices.

A configured group/global limit returns `voice_limit_reached` under `reject_new`. The retained storage ceiling remains `capacity_exceeded` when no configured semantic limit caused the rejection.

## Deterministic stealing

`steal_oldest` uses a monotonically increasing semantic start order. Selection is deterministic:

1. if the incoming voice hits its group limit, select only from that group;
2. otherwise, if the global semantic/storage ceiling is hit, select from all active voices;
3. choose the lowest start order;
4. use slot index only as a stable final tie-break.

No distance, loudness, pointer address, wall clock, backend device state, or hash-map iteration order participates in the decision.

The stolen voice publishes `AudioEventType2D::Stolen` before the replacement `Started` event. The reason is finite and machine-readable:

- `group_voice_limit`;
- `global_voice_limit`.

The stolen handle becomes stale through the existing generation increment before the replacement voice becomes observable.

## Fail-closed event capacity

A normal start requires one event. A steal+replacement requires two.

`Play()` preflights that full event requirement before retaining or deactivating the old voice. If the retained event budget cannot hold both `Stolen` and `Started`, the command returns `capacity_exceeded`, publishes nothing, and leaves the old semantic voice active.

## Autoplay

`StartAutoplay()` remains an atomic/fail-closed batch surface. It computes the complete required voice count and per-group counts before starting anything.

Even when `steal_oldest` is configured, autoplay does **not** silently evict pre-existing semantic voices to make a batch fit. A batch that would exceed configured limits returns `voice_limit_reached` with zero new voices started.

This keeps startup authoring deterministic and avoids partial scene-start behavior.

## Metrics

Cheap structural metrics now include:

- active voices per group;
- stolen voice count;
- voice-limit rejection count;
- existing retained capacity/high-water/event-capacity counters.

These are intended for later #91 aggregation and are not JSON/snapshot work on the playback hot path.

## Performance / ownership

- ordinary `Step()` is unchanged and performs no voice-selection scan;
- admission scans active retained voice slots only when stealing is actually required;
- no filesystem/decode/device work is introduced;
- no semantic strings, reflection, generic Variant values, or heap allocation are required by the steady playback step after capacities are retained;
- stealing reuses the released generational slot instead of growing storage beyond the retained voice budget.

## Validation

`AudioVoiceLimitTests` covers:

- group and global `reject_new` behavior;
- oldest-within-group stealing;
- oldest-across-groups global stealing;
- stale stolen handles;
- two-event fail-closed steal preflight;
- autoplay limit preflight with no hidden eviction;
- per-group active/stolen/rejected metric evidence.

The tests run inside the existing `trace2d_audio_tests` sanitizer-headless target.

## Next #77 slice

AUDIO3 does **not** complete #77.

The next bounded slice must compose AUDIO2 prepared/streaming PCM with the selected SDL3 output boundary and add:

1. playback device + semantic-to-physical voice synchronization;
2. bounded retained stream refill/ring buffering outside the gameplay thread;
3. pause/resume/stop, effective volume and pitch propagation;
4. device suspend/resume/loss/default-device recovery;
5. exact Trace2D-owned stream/device buffer accounting;
6. real physical-output smoke evidence.

That backend consumes AUDIO3 admission decisions; it does not run its own hidden stealing policy. Spatial attenuation/panning and DSP remain deferred unless #329 demonstrates a concrete need.
