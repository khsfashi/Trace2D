# Audio2D AUDIO1 — canonical clip identity and deterministic semantic playback

Issue: #359, first implementation slice of #77.

## Purpose

AUDIO1 establishes the authority boundary required before physical audio output exists:

1. `ResourceRegistry` owns project-relative, generation-safe `AudioClipResource` identity and immutable imported timing metadata;
2. authored `AudioSource2D` owns finite gameplay-facing playback intent;
3. `AudioSystem2D` owns retained semantic voice state and events;
4. a later physical backend consumes that state as presentation and must never become gameplay truth.

Headless builds therefore exercise the same play/pause/resume/stop, loop, completion, lifecycle and capacity semantics even when no device is opened.

## External reference decision

AUDIO1 follows `docs/EXTERNAL_REFERENCE_PROTOCOL.md`.

### SDL3 — ADOPT for the later physical backend

Official references:

- https://wiki.libsdl.org/SDL3/SDL_AudioStream
- https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream
- https://wiki.libsdl.org/SDL3/SDL_CreateAudioStream

SDL3's `SDL_AudioStream` is a suitable physical conversion/buffering/device-facing substrate because SDL3 centers audio playback around streams and can perform format conversion/resampling while binding streams to opened devices.

AUDIO1 deliberately does **not** open a device or expose an `SDL_AudioStream` through public Trace2D state. That object will be backend-owned derived presentation state in the next physical slice.

### Godot AudioStreamPlayer — ADAPT the practical vocabulary

Official reference:

- https://docs.godotengine.org/en/4.4/classes/class_audiostreamplayer.html

Trace2D adapts the practical finite vocabulary—clip/stream selection, play/stop/pause, volume, pitch, loop-like stream behavior, bus/group selection, autoplay and an explicit polyphony/voice-limit direction—without adopting Node/StringName lookup or making engine-device state semantic authority.

### Rejected from AUDIO1

- DSP/effect graph;
- positional/spatial attenuation and panning;
- codec-specific decoder API;
- decode/stream worker pool;
- waveform-based gameplay verification;
- microphone/capture;
- implicit oldest-voice stealing.

These remain evidence-gated rather than being implemented merely because mature engines expose them.

## `AudioClipResource` ownership

`AudioClipResource` adds resource domain `audio_clip` to #86 with:

- `preload` or `stream` preparation intent;
- sample rate;
- channel count;
- total source frame count;
- encoded source byte size;
- required canonical-metadata retention.

The registry does **not** retain a second copy of encoded source bytes and does not retain decoded PCM. `encodedByteSize` is evidence about the imported source, not owned byte storage. The physical audio layer will own prepared decoded/stream buffers and must account that memory separately.

This split prevents a long music asset from becoming `source bytes + registry duplicate + decoder duplicate + device buffer` simply to preserve semantic identity.

Current imported metadata bounds are intentionally finite:

- sample rate: `1..384000 Hz`;
- channel count: `1..8`;
- frame count: non-zero;
- encoded source size: non-zero.

`FindReadyAudioClip()` performs the same canonical project-relative identity lookup as publication. It performs no file discovery, decode or republish operation.

## Authored `AudioSource2D`

Schema `trace2d.audiosource2d` version 1 contains exactly:

- `clip`: bounded portable project-relative resource reference;
- `volume`: linear `[0,1]`;
- `pitch`: finite `(0,8]`;
- `loop`: boolean;
- `autoplay`: boolean;
- `group`: `master | music | sfx | ui`.

The pitch upper bound is an explicit AUDIO1 safety bound so one caller-supplied semantic step cannot create effectively unbounded source-frame progression from an arbitrary float. Representative product evidence can justify changing the bound later; physical backend behavior must match the semantic result rather than silently expanding it.

## Semantic voice authority

`AudioSystem2D` uses reusable generation-safe voice slots. A voice snapshots the source's finite playback parameters and the exact `AudioClipResource` handle at `Play()` time.

Commands:

- `Play(entity)`;
- `Pause(voice)`;
- `Resume(voice)`;
- `Stop(voice)`;
- `SetGroupVolume(group, value)`;
- one-shot `StartAutoplay()` after scene setup.

State inspection exposes source/effective volume, pitch, loop flag, source-frame playhead and completed-loop count. `master` multiplies every non-master group's gain, giving a bounded bus-like control surface without a graph or string lookup.

## Deterministic caller-supplied time

`Step(std::chrono::nanoseconds)` never queries wall-clock time. The caller supplies semantic elapsed time. Source-frame progression is derived from:

```text
delta_seconds * clip.sample_rate * voice.pitch
```

Loop crossings in one step are aggregated into one `looped` event with `loopsCrossed=N`. This keeps event volume bounded even when one supplied delta spans multiple iterations.

AUDIO1 does not claim universal cross-platform bit-identical floating-point playhead replay. The authoritative externally visible boundaries are finite command/state transitions, stable slot ordering, loop/completion events and fail-closed capacity behavior. Physical output may interpolate/sample independently but cannot change those semantic transitions.

## Stable event and lifecycle rules

Events are retained in caller-inspected order with a monotonically increasing sequence:

- `started`;
- `paused`;
- `resumed`;
- `stopped`;
- `looped`;
- `finished`;
- `detached`.

Stepping visits active voice slots in stable slot order. Backend callback order is never observable because AUDIO1 has no backend callback authority.

Lifecycle is explicit:

- destroying a source entity detaches its live voices on the next semantic step with reason `entity_destroyed`;
- unloading/clearing the exact clip generation detaches its live voices on the next semantic step with reason `resource_unavailable`;
- a finished/stopped/detached voice increments its slot generation, making old handles stale.

No stale resource pointer or entity index remains usable as a live voice.

## Capacity and allocation policy

Callers prepare retained storage with:

- `ReserveVoices(capacity)`;
- `ReserveEvents(capacity)`.

At prepared capacity, ordinary `Step()`:

- performs no file discovery;
- performs no decode;
- performs no semantic string lookup;
- performs no required heap allocation;
- scans retained voice slots in `O(V)` time for `V` allocated slots.

Before changing any voice playhead, `Step()` counts all events that the step would need. If retained event capacity is insufficient, it returns `capacity_exceeded`, publishes zero partial events and advances zero voices. The caller may reserve and retry the same delta.

`StartAutoplay()` similarly preflights required voice/event capacity before starting any authored autoplay voice.

AUDIO1 deliberately fails closed when voice capacity is exhausted. It does not guess a stealing victim. The next physical slice must define a measured, explicit voice-limit/stealing policy before implementing automatic stealing.

## Metrics available for #91

`AudioMetrics2D` exposes cheap scalar evidence:

- retained voice/event capacity;
- allocated/active/high-water voice counts;
- current published event count;
- command/failure counts;
- successful semantic step count;
- loop/completion/detach counts;
- event-capacity failure count.

No normal step builds a diagnostic report or allocates strings.

## AUDIO2 continuation for #77

AUDIO1 is not completion of #77. The next bounded slice should add the physical production path while preserving this semantic contract:

1. actual project audio import/decode preparation;
2. short-SFX preload and long-music streaming behavior driven by `AudioClipLoadPolicy`;
3. SDL3 `SDL_AudioStream` device-facing presentation;
4. decoded/stream/device-buffer memory accounting;
5. explicit global/per-group voice limits and deterministic/observable stealing policy;
6. device suspend/resume/loss recovery;
7. practical fades where the representative combat/music workflow proves they are needed;
8. physical-output smoke evidence while retaining all semantic tests headlessly.

2D spatial attenuation/panning remains deferred unless #329 or another representative product demonstrates a concrete need. After the complete bounded #77 Audio V1 is green, the fixed lane advances to #329 Combat Product Proof rather than speculative Audio breadth.
