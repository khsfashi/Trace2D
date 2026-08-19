#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>

namespace trace2d::audio
{
namespace
{
using namespace std::chrono_literals;

[[nodiscard]] assets::AudioClipResource MakeClip(
    const std::uint64_t frameCount = 48000U,
    const assets::AudioClipLoadPolicy loadPolicy = assets::AudioClipLoadPolicy::Preload)
{
    assets::AudioClipResource clip{};
    clip.loadPolicy = loadPolicy;
    clip.sampleRateHz = 48000U;
    clip.channelCount = 2U;
    clip.frameCount = frameCount;
    clip.encodedByteSize = 4096U;
    return clip;
}

[[nodiscard]] AudioSource2D MakeSource(
    std::string clipReference = "audio/sfx/hit.wav",
    const float volume = 1.0F,
    const float pitch = 1.0F,
    const bool loop = false,
    const bool autoplay = false,
    const AudioGroup2D group = AudioGroup2D::Sfx)
{
    AudioSource2D source{};
    source.clipReference = std::move(clipReference);
    source.volume = volume;
    source.pitch = pitch;
    source.loop = loop;
    source.autoplay = autoplay;
    source.group = group;
    return source;
}

[[nodiscard]] scene::SemanticValue TextValue(std::string value, const scene::SemanticValueKind kind)
{
    scene::SemanticValue semantic{};
    semantic.kind = kind;
    semantic.textValue = std::move(value);
    return semantic;
}

[[nodiscard]] scene::SemanticValue FloatValue(const double value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Float;
    semantic.floatValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue BooleanValue(const bool value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Boolean;
    semantic.booleanValue = value;
    return semantic;
}

[[nodiscard]] scene::EntityId AddSourceEntity(
    scene::Scene& scene,
    const scene::ComponentTypeHandle<AudioSource2D> sourceType,
    const std::string& semanticId,
    AudioSource2D source)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = semanticId;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));
    (void)scene.AddComponent(entity, sourceType, std::move(source));
    return entity;
}
} // namespace

TEST(Audio2DResource, PublishesCanonicalMetadataLookupAndMemoryEvidence)
{
    assets::ResourceRegistry resources{"."};
    const auto first = resources.PublishAudioClip("audio\\sfx\\hit.wav", MakeClip());
    ASSERT_TRUE(first.Succeeded());
    EXPECT_FALSE(first.reusedExisting);
    EXPECT_EQ(first.handle.domain, assets::ResourceTypeDomain::AudioClip);

    const assets::AudioClipResource* const resolved = resources.Resolve(first.handle);
    ASSERT_NE(resolved, nullptr);
    EXPECT_EQ(resolved->sampleRateHz, 48000U);
    EXPECT_EQ(resolved->channelCount, 2U);
    EXPECT_EQ(resolved->frameCount, 48000U);
    EXPECT_EQ(resolved->loadPolicy, assets::AudioClipLoadPolicy::Preload);

    const auto found = resources.FindReadyAudioClip("./audio/sfx/hit.wav");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, first.handle);

    const auto duplicate = resources.PublishAudioClip("audio/sfx/hit.wav", MakeClip());
    ASSERT_TRUE(duplicate.Succeeded());
    EXPECT_TRUE(duplicate.reusedExisting);
    EXPECT_EQ(duplicate.handle, first.handle);

    const auto snapshot = resources.Inspect(first.handle.Untyped());
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->identity.domain, assets::ResourceTypeDomain::AudioClip);
    EXPECT_EQ(snapshot->identity.canonicalReference, "audio/sfx/hit.wav");
    EXPECT_GE(snapshot->memory.knownRetainedCpuBytes, sizeof(assets::AudioClipResource));
    EXPECT_TRUE(snapshot->memory.cpuPayloadResident);
    EXPECT_FALSE(snapshot->memory.rendererResident);
    EXPECT_EQ(snapshot->memory.knownRendererGpuBytes, 0U);

    const assets::ResourceOperationResult unloaded = resources.Unload(first.handle.Untyped());
    EXPECT_TRUE(unloaded.Succeeded());
    EXPECT_EQ(resources.Resolve(first.handle), nullptr);
    EXPECT_FALSE(resources.FindReadyAudioClip("audio/sfx/hit.wav").has_value());
}

TEST(Audio2DResource, RejectsMalformedImportedMetadata)
{
    assets::ResourceRegistry resources{"."};

    assets::AudioClipResource invalid = MakeClip();
    invalid.sampleRateHz = assets::MaximumAudioClipSampleRateHz + 1U;
    const auto badRate = resources.PublishAudioClip("audio/bad-rate.wav", invalid);
    ASSERT_FALSE(badRate.Succeeded());
    ASSERT_TRUE(badRate.diagnostic.has_value());
    EXPECT_EQ(badRate.diagnostic->code, assets::ResourceErrorCode::InvalidPayload);

    invalid = MakeClip();
    invalid.channelCount = assets::MaximumAudioClipChannelCount + 1U;
    EXPECT_FALSE(resources.PublishAudioClip("audio/bad-channels.wav", invalid).Succeeded());

    invalid = MakeClip();
    invalid.frameCount = 0U;
    EXPECT_FALSE(resources.PublishAudioClip("audio/empty.wav", invalid).Succeeded());

    invalid = MakeClip();
    invalid.encodedByteSize = 0U;
    EXPECT_FALSE(resources.PublishAudioClip("audio/no-source.wav", invalid).Succeeded());
}

TEST(Audio2DComponent, AuthoredContractRoundTripsAndRejectsTraversal)
{
    scene::ComponentRegistry registry{};
    const AudioComponentTypes2D types = RegisterAudio2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = "music";
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));

    scene::ComponentAuthoringObject authored{};
    authored.fields.push_back({"clip", TextValue("audio/music/theme.ogg", scene::SemanticValueKind::ResourceReference)});
    authored.fields.push_back({"volume", FloatValue(0.75)});
    authored.fields.push_back({"pitch", FloatValue(1.25)});
    authored.fields.push_back({"loop", BooleanValue(true)});
    authored.fields.push_back({"autoplay", BooleanValue(true)});
    authored.fields.push_back({"group", TextValue("music", scene::SemanticValueKind::EnumName)});

    std::string error{};
    EXPECT_EQ(
        scene.AddAuthoredComponent(entity, "trace2d.audiosource2d", 1U, authored, error),
        scene::ComponentAttachResult::Success)
        << error;

    const AudioSource2D* const source = scene.TryGetComponent(entity, types.source);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->clipReference, "audio/music/theme.ogg");
    EXPECT_FLOAT_EQ(source->volume, 0.75F);
    EXPECT_FLOAT_EQ(source->pitch, 1.25F);
    EXPECT_TRUE(source->loop);
    EXPECT_TRUE(source->autoplay);
    EXPECT_EQ(source->group, AudioGroup2D::Music);

    const auto snapshots = scene.SerializeAuthoredComponents(entity, error);
    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots[0].typeId, "trace2d.audiosource2d");
    EXPECT_EQ(snapshots[0].schemaVersion, 1U);
    const scene::SemanticValue* const serializedClip = snapshots[0].data.Find("clip");
    ASSERT_NE(serializedClip, nullptr);
    EXPECT_EQ(serializedClip->kind, scene::SemanticValueKind::ResourceReference);
    EXPECT_EQ(serializedClip->textValue, "audio/music/theme.ogg");

    scene::EntityDescriptor invalidDescriptor{};
    invalidDescriptor.semanticId = "invalid";
    const scene::EntityId invalidEntity = scene.CreateEntity(std::move(invalidDescriptor));
    EXPECT_THROW(
        (void)scene.AddComponent(invalidEntity, types.source, MakeSource("../outside.wav")),
        std::invalid_argument);
}

TEST(Audio2DPlayback, PlayPauseResumePitchAndGroupVolumeAreHeadlessSemanticState)
{
    scene::ComponentRegistry registry{};
    const AudioComponentTypes2D types = RegisterAudio2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = AddSourceEntity(
        scene,
        types.source,
        "speaker",
        MakeSource("audio/sfx/hit.wav", 0.5F, 2.0F, false, false, AudioGroup2D::Sfx));

    assets::ResourceRegistry resources{"."};
    ASSERT_TRUE(resources.PublishAudioClip("audio/sfx/hit.wav", MakeClip()).Succeeded());

    AudioSystem2D audio{scene, resources, types.source};
    ASSERT_TRUE(audio.ReserveVoices(2U));
    ASSERT_TRUE(audio.ReserveEvents(16U));
    EXPECT_EQ(audio.SetGroupVolume(AudioGroup2D::Master, 0.8F), AudioCommandResult2D::Success);
    EXPECT_EQ(audio.SetGroupVolume(AudioGroup2D::Sfx, 0.5F), AudioCommandResult2D::Success);

    const AudioPlayResult2D play = audio.Play(entity);
    ASSERT_EQ(play.result, AudioCommandResult2D::Success);
    ASSERT_TRUE(play.voice.IsValid());
    ASSERT_EQ(audio.Events().size(), 1U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Started);

    auto state = audio.InspectVoice(play.voice);
    ASSERT_TRUE(state.has_value());
    EXPECT_FLOAT_EQ(state->sourceVolume, 0.5F);
    EXPECT_NEAR(state->effectiveVolume, 0.2F, 0.00001F);
    EXPECT_DOUBLE_EQ(state->positionFrames, 0.0);

    audio.ClearEvents();
    const AudioStepReport2D firstStep = audio.Step(100ms);
    EXPECT_EQ(firstStep.result, AudioStepResult2D::Success);
    EXPECT_EQ(firstStep.generatedEventCount, 0U);
    state = audio.InspectVoice(play.voice);
    ASSERT_TRUE(state.has_value());
    EXPECT_NEAR(state->positionFrames, 9600.0, 0.001);

    EXPECT_EQ(audio.Pause(play.voice), AudioCommandResult2D::Success);
    audio.ClearEvents();
    EXPECT_EQ(audio.Step(500ms).result, AudioStepResult2D::Success);
    state = audio.InspectVoice(play.voice);
    ASSERT_TRUE(state.has_value());
    EXPECT_NEAR(state->positionFrames, 9600.0, 0.001);

    EXPECT_EQ(audio.Resume(play.voice), AudioCommandResult2D::Success);
    audio.ClearEvents();
    const AudioStepReport2D finishStep = audio.Step(400ms);
    EXPECT_EQ(finishStep.result, AudioStepResult2D::Success);
    ASSERT_EQ(audio.Events().size(), 1U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Finished);
    EXPECT_FALSE(audio.InspectVoice(play.voice).has_value());

    const AudioMetrics2D metrics = audio.Metrics();
    EXPECT_EQ(metrics.activeVoiceCount, 0U);
    EXPECT_EQ(metrics.voiceHighWatermark, 1U);
    EXPECT_EQ(metrics.completionEventCount, 1U);
}

TEST(Audio2DPlayback, LoopAggregationAndEntityDespawnProduceStableSemanticEvents)
{
    scene::ComponentRegistry registry{};
    const AudioComponentTypes2D types = RegisterAudio2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = AddSourceEntity(
        scene,
        types.source,
        "looping",
        MakeSource("audio/sfx/loop.wav", 1.0F, 2.0F, true));

    assets::ResourceRegistry resources{"."};
    ASSERT_TRUE(resources.PublishAudioClip("audio/sfx/loop.wav", MakeClip()).Succeeded());

    AudioSystem2D audio{scene, resources, types.source};
    ASSERT_TRUE(audio.ReserveVoices(1U));
    ASSERT_TRUE(audio.ReserveEvents(4U));
    const AudioPlayResult2D play = audio.Play(entity);
    ASSERT_EQ(play.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    const AudioStepReport2D loopStep = audio.Step(1250ms);
    ASSERT_EQ(loopStep.result, AudioStepResult2D::Success);
    ASSERT_EQ(audio.Events().size(), 1U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Looped);
    EXPECT_EQ(audio.Events()[0].loopsCrossed, 2U);
    const auto state = audio.InspectVoice(play.voice);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->completedLoops, 2U);
    EXPECT_NEAR(state->positionFrames, 24000.0, 0.001);

    audio.ClearEvents();
    ASSERT_TRUE(scene.DestroyEntity(entity));
    const AudioStepReport2D detachStep = audio.Step(0ns);
    ASSERT_EQ(detachStep.result, AudioStepResult2D::Success);
    ASSERT_EQ(audio.Events().size(), 1U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Detached);
    EXPECT_EQ(audio.Events()[0].reason, AudioEventReason2D::EntityDestroyed);
    EXPECT_FALSE(audio.InspectVoice(play.voice).has_value());
}

TEST(Audio2DPlayback, ResourceUnloadDetachesVoiceOnNextSemanticStep)
{
    scene::ComponentRegistry registry{};
    const AudioComponentTypes2D types = RegisterAudio2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = AddSourceEntity(scene, types.source, "speaker", MakeSource());

    assets::ResourceRegistry resources{"."};
    const auto clip = resources.PublishAudioClip("audio/sfx/hit.wav", MakeClip());
    ASSERT_TRUE(clip.Succeeded());

    AudioSystem2D audio{scene, resources, types.source};
    ASSERT_TRUE(audio.ReserveVoices(1U));
    ASSERT_TRUE(audio.ReserveEvents(4U));
    const AudioPlayResult2D play = audio.Play(entity);
    ASSERT_EQ(play.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    ASSERT_TRUE(resources.Unload(clip.handle.Untyped()).Succeeded());
    ASSERT_EQ(audio.Step(0ns).result, AudioStepResult2D::Success);
    ASSERT_EQ(audio.Events().size(), 1U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Detached);
    EXPECT_EQ(audio.Events()[0].reason, AudioEventReason2D::ResourceUnavailable);
    EXPECT_FALSE(audio.InspectVoice(play.voice).has_value());
}

TEST(Audio2DPlayback, EventCapacityFailurePublishesNoPartialStateAndRetryIsStable)
{
    scene::ComponentRegistry registry{};
    const AudioComponentTypes2D types = RegisterAudio2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = AddSourceEntity(scene, types.source, "polyphonic", MakeSource());

    assets::ResourceRegistry resources{"."};
    ASSERT_TRUE(resources.PublishAudioClip("audio/sfx/hit.wav", MakeClip()).Succeeded());

    AudioSystem2D audio{scene, resources, types.source};
    ASSERT_TRUE(audio.ReserveVoices(2U));
    ASSERT_TRUE(audio.ReserveEvents(1U));

    const AudioPlayResult2D first = audio.Play(entity);
    ASSERT_EQ(first.result, AudioCommandResult2D::Success);
    audio.ClearEvents();
    const AudioPlayResult2D second = audio.Play(entity);
    ASSERT_EQ(second.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    const AudioStepReport2D failed = audio.Step(1s);
    EXPECT_EQ(failed.result, AudioStepResult2D::CapacityExceeded);
    EXPECT_EQ(failed.generatedEventCount, 0U);
    EXPECT_EQ(failed.requiredEventCapacity, 2U);
    ASSERT_TRUE(audio.Events().empty());

    const auto firstBeforeRetry = audio.InspectVoice(first.voice);
    const auto secondBeforeRetry = audio.InspectVoice(second.voice);
    ASSERT_TRUE(firstBeforeRetry.has_value());
    ASSERT_TRUE(secondBeforeRetry.has_value());
    EXPECT_DOUBLE_EQ(firstBeforeRetry->positionFrames, 0.0);
    EXPECT_DOUBLE_EQ(secondBeforeRetry->positionFrames, 0.0);

    ASSERT_TRUE(audio.ReserveEvents(2U));
    const AudioStepReport2D retried = audio.Step(1s);
    EXPECT_EQ(retried.result, AudioStepResult2D::Success);
    EXPECT_EQ(retried.generatedEventCount, 2U);
    ASSERT_EQ(audio.Events().size(), 2U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Finished);
    EXPECT_EQ(audio.Events()[1].type, AudioEventType2D::Finished);
    EXPECT_EQ(audio.Events()[0].voice, first.voice);
    EXPECT_EQ(audio.Events()[1].voice, second.voice);
    EXPECT_LT(audio.Events()[0].sequence, audio.Events()[1].sequence);
}

TEST(Audio2DPlayback, AutoplayPreflightIsFailClosedAtRetainedCapacity)
{
    scene::ComponentRegistry registry{};
    const AudioComponentTypes2D types = RegisterAudio2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    (void)AddSourceEntity(scene, types.source, "music-a", MakeSource("audio/music/theme.ogg", 1.0F, 1.0F, true, true, AudioGroup2D::Music));
    (void)AddSourceEntity(scene, types.source, "music-b", MakeSource("audio/music/theme.ogg", 1.0F, 1.0F, true, true, AudioGroup2D::Music));

    assets::ResourceRegistry resources{"."};
    ASSERT_TRUE(resources.PublishAudioClip(
        "audio/music/theme.ogg",
        MakeClip(480000U, assets::AudioClipLoadPolicy::Stream)).Succeeded());

    AudioSystem2D audio{scene, resources, types.source};
    ASSERT_TRUE(audio.ReserveVoices(1U));
    ASSERT_TRUE(audio.ReserveEvents(1U));
    const AudioAutoplayReport2D failed = audio.StartAutoplay();
    EXPECT_EQ(failed.result, AudioAutoplayResult2D::CapacityExceeded);
    EXPECT_EQ(failed.startedVoiceCount, 0U);
    EXPECT_EQ(failed.requiredVoiceCapacity, 2U);
    EXPECT_EQ(failed.requiredEventCapacity, 2U);
    EXPECT_EQ(audio.Metrics().activeVoiceCount, 0U);
    EXPECT_TRUE(audio.Events().empty());

    ASSERT_TRUE(audio.ReserveVoices(2U));
    ASSERT_TRUE(audio.ReserveEvents(2U));
    const AudioAutoplayReport2D started = audio.StartAutoplay();
    EXPECT_EQ(started.result, AudioAutoplayResult2D::Success);
    EXPECT_EQ(started.startedVoiceCount, 2U);
    EXPECT_EQ(audio.Metrics().activeVoiceCount, 2U);
    EXPECT_EQ(audio.Events().size(), 2U);

    const AudioAutoplayReport2D repeated = audio.StartAutoplay();
    EXPECT_EQ(repeated.result, AudioAutoplayResult2D::AlreadyStarted);
    EXPECT_EQ(audio.Metrics().activeVoiceCount, 2U);
}
} // namespace trace2d::audio
