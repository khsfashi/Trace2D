# Audio2D AUDIO2 — codec preparation and bounded streaming decode

Issue: #361, second implementation slice of #77.

## Purpose

AUDIO1 established deterministic semantic playback. AUDIO2 adds the derived audio-data preparation layer required before physical output:

```text
#86 AudioClipResource
  -> explicit AUDIO2 source open / metadata validation
  -> preload: retained interleaved float PCM
     or
     stream: retained decoder + caller-owned bounded PCM chunk
  -> later SDL3 physical-output backend
```

AUDIO2 is not gameplay authority and does not open an audio device. `AudioSystem2D` remains the semantic voice/event/state authority and its `Step()` path performs no filesystem or decode work.

## Reuse-before-build decision

Reviewed against current primary sources on 2026-08-20.

### miniaudio 0.11.25 — ADOPT decoder only

The repository's pinned vcpkg baseline resolves `miniaudio` 0.11.25. The port is header-only and upstream offers permissive Unlicense/MIT-0 terms.

Trace2D adopts only `ma_decoder` file opening, PCM-frame reads, length queries and seeking. `MA_NO_DEVICE_IO`, `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH`, and `MA_NO_RESOURCE_MANAGER` keep the implementation on the codec/data-source side of the boundary.

The stock miniaudio decoder supports WAV, FLAC and MP3. Those are the AUDIO2 V1 decoded source formats. Ogg/Vorbis is **not** claimed by this slice; adding another decoder is evidence-gated rather than silently pretending the authoring reference extension implies runtime support.

### SDL3 — DEFER device/output to the next #77 slice

SDL3 remains the selected physical device/mix boundary from AUDIO1. SDL3 audio streams provide format conversion/resampling, per-stream gain and frequency ratio, and can bind/mix multiple streams into a playback device. AUDIO2 deliberately does not duplicate that role with miniaudio's device/engine layer.

## Canonical resource authority

AUDIO2 begins from an already-ready generation-safe `AudioClipResource` handle.

The source path is derived only during explicit preparation/open from:

```text
ResourceRegistry::ProjectRoot()
 + ResourceSnapshot.identity.canonicalReference
```

The decoder then validates actual decoded sample rate, channel count and PCM frame count against the canonical resource. A mismatch fails closed as `metadata_mismatch`; decoded data never silently rewrites canonical #86 truth.

If the resource handle is stale/unavailable, preparation fails before source access.

## Preload policy

`AudioClipLoadPolicy::Preload` uses `AudioClipPreparation2D::PreparePreloaded()`.

The source is opened and validated, then decoded once to interleaved `float` PCM using the canonical sample rate/channel count. The returned `PreparedAudioClip2D` owns the PCM vector.

Exact owned evidence includes:

- decoded frame count;
- PCM logical bytes;
- retained PCM vector capacity bytes;
- one explicit preparation read count.

After preparation, the retained object performs no source-file I/O. This is the intended short-SFX path for the later physical backend.

## Stream policy

`AudioClipLoadPolicy::Stream` uses `AudioClipPreparation2D::OpenStream()`.

The returned `StreamingAudioClip2D` retains decoder state but no Trace2D-owned PCM buffer. Each `ReadFrames()` receives a caller-owned span and a requested frame count. The call fails before decoding if the span cannot hold `requestedFrames * channelCount` samples.

The stream supports explicit PCM-frame `Seek()` with canonical frame-range validation.

Important boundary:

> `ReadFrames()` may block on source-file I/O. It is a setup/stream-worker operation and must never be called from `AudioSystem2D::Step()` or an ordinary gameplay/frame hot path.

The next physical-output slice owns the worker/ring-buffer scheduling needed to keep device refill work bounded without blocking the gameplay thread.

## Memory accounting

AUDIO2 does not invent an exact number for dependency-internal opaque allocations.

Metrics therefore separate:

- exact Trace2D-owned PCM bytes/capacity;
- exact retained decoder object size;
- dependency-internal bytes as explicitly `unknown`;
- whether future reads may perform blocking source I/O.

For streams, PCM chunk memory is caller-owned and therefore remains zero in `StreamingAudioClip2D` retained-PCM metrics.

## Failure vocabulary

The preparation surface is finite and machine-readable:

- `success`;
- `resource_unavailable`;
- `policy_mismatch`;
- `source_open_failed`;
- `metadata_mismatch`;
- `decode_failed`;
- `invalid_buffer`;
- `seek_out_of_range`;
- `seek_failed`.

Setup diagnostics may allocate strings. Normal AUDIO1 semantic stepping still does not build these diagnostics.

## Validation

`AudioClipPreparation2DTests` creates small PCM16 WAV sources in each test's temporary project root and proves:

- real preload file decoding to retained float PCM;
- exact owned PCM byte/capacity evidence;
- bounded caller-owned stream reads;
- stream seeking and range rejection;
- no retained stream PCM allocation;
- load-policy mismatch rejection;
- canonical metadata mismatch rejection;
- stale resource rejection before decode.

The existing AUDIO1 semantic suite remains unchanged and continues to be the headless playback authority.

## Next #77 slice

AUDIO2 does **not** complete #77. The next bounded slice should compose these prepared/streamed PCM paths with SDL3 physical presentation and add the remaining production lifecycle:

1. SDL3 playback device + per-voice stream/output ownership;
2. worker/ring-buffer refill for `stream` clips, with bounded retained memory and no gameplay-thread file I/O;
3. semantic voice -> physical voice synchronization for start/pause/resume/stop, volume and pitch;
4. explicit global/per-group voice limits plus deterministic observable stealing;
5. device suspend/resume/loss/default-device recovery;
6. physical output smoke evidence;
7. only the fade semantics demonstrated necessary by the representative workflow.

Spatial attenuation/panning remains deferred to #329 evidence. Only after the complete #77 Audio V1 is green does the core lane advance to #329 Combat Product Proof.
