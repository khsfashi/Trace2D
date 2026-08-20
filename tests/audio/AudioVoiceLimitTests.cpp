#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioComponents2D.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>

#include <gtest/gtest.h>

#include <string>

namespace trace2d::audio
{
namespace
{
[[nodiscard]] assets::AudioClipResource MakeVoiceLimitClip()
{
    assets::AudioClipResource clip{};
    clip.loadPolicy = assets::AudioClipLoadPolicy::Preload;
    clip.sampleRateHz = 48000U;
    clip.channelCount = 2U;
    clip.frameCount = 48000U;
    clip.encodedByteSize = 4096U;
    return clip;
}

[[nodiscard]] AudioSource2D MakeVoiceLimitSource(
    const AudioGroup2D group,
    const bool autoplay = false)
{
    AudioSource2D source{};
    source.clipReference = "audio/sfx/voice-limit.wav";
    source.group = group;
    source.autoplay = autoplay;
    return source;
}

[[nodiscard]] scene::EntityId AddVoiceLimitSource(
    scene::Scene& scene,
    const scene::ComponentTypeHandle<AudioSource2D> sourceType,
    const std::string& semanticId,
    const AudioGroup2D group,
    const bool autoplay = false)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = semanticId;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));
    (void)scene.AddComponent(entity, sourceType, MakeVoiceLimitSource(group, autoplay));
    return entity;
}

struct VoiceLimitFixture final
{
    scene::ComponentRegistry registry{};
    AudioComponentTypes2D types{};
    scene::Scene scene;
    assets::ResourceRegistry resources{"."};

    VoiceLimitFixture()
        : types{RegisterAudio2DComponents(registry)}
        , scene{FreezeAndReturnRegistry()}
    {
        EXPECT_TRUE(resources.PublishAudioClip("audio/sfx/voice-limit.wav", MakeVoiceLimitClip()).Succeeded());
    }

private:
    [[nodiscard]] scene::ComponentRegistry& FreezeAndReturnRegistry()
    {
        registry.Freeze();
        return registry;
    }
};
} // namespace

TEST(Audio2DVoiceLimits, RejectNewEnforcesGroupThenGlobalLimits)
{
    VoiceLimitFixture fixture{};
    const scene::EntityId sfxA = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "sfx-a", AudioGroup2D::Sfx);
    const scene::EntityId sfxB = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "sfx-b", AudioGroup2D::Sfx);
    const scene::EntityId music = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "music", AudioGroup2D::Music);
    const scene::EntityId ui = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "ui", AudioGroup2D::Ui);

    AudioSystem2D audio{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(audio.ReserveVoices(4U));
    ASSERT_TRUE(audio.ReserveEvents(16U));

    AudioVoiceLimits2D limits{};
    limits.globalLimit = 2U;
    limits.groupLimits[static_cast<std::size_t>(AudioGroup2D::Sfx)] = 1U;
    limits.overflowPolicy = AudioVoiceOverflowPolicy2D::RejectNew;
    ASSERT_TRUE(audio.SetVoiceLimits(limits));

    EXPECT_EQ(audio.Play(sfxA).result, AudioCommandResult2D::Success);
    EXPECT_EQ(audio.Play(sfxB).result, AudioCommandResult2D::VoiceLimitReached);
    EXPECT_EQ(audio.Play(music).result, AudioCommandResult2D::Success);
    EXPECT_EQ(audio.Play(ui).result, AudioCommandResult2D::VoiceLimitReached);

    const AudioMetrics2D metrics = audio.Metrics();
    EXPECT_EQ(metrics.activeVoiceCount, 2U);
    EXPECT_EQ(metrics.activeVoiceCountByGroup[static_cast<std::size_t>(AudioGroup2D::Sfx)], 1U);
    EXPECT_EQ(metrics.activeVoiceCountByGroup[static_cast<std::size_t>(AudioGroup2D::Music)], 1U);
    EXPECT_EQ(metrics.voiceLimitRejectCount, 2U);
    EXPECT_EQ(metrics.stolenVoiceCount, 0U);
}

TEST(Audio2DVoiceLimits, StealOldestUsesStableGroupOrderAndInvalidatesOldHandle)
{
    VoiceLimitFixture fixture{};
    const scene::EntityId firstEntity = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "first", AudioGroup2D::Sfx);
    const scene::EntityId secondEntity = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "second", AudioGroup2D::Sfx);
    const scene::EntityId replacementEntity = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "replacement", AudioGroup2D::Sfx);

    AudioSystem2D audio{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(audio.ReserveVoices(3U));
    ASSERT_TRUE(audio.ReserveEvents(16U));

    AudioVoiceLimits2D limits{};
    limits.globalLimit = 3U;
    limits.groupLimits[static_cast<std::size_t>(AudioGroup2D::Sfx)] = 2U;
    limits.overflowPolicy = AudioVoiceOverflowPolicy2D::StealOldest;
    ASSERT_TRUE(audio.SetVoiceLimits(limits));

    const AudioPlayResult2D first = audio.Play(firstEntity);
    const AudioPlayResult2D second = audio.Play(secondEntity);
    ASSERT_EQ(first.result, AudioCommandResult2D::Success);
    ASSERT_EQ(second.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    const AudioPlayResult2D replacement = audio.Play(replacementEntity);
    ASSERT_EQ(replacement.result, AudioCommandResult2D::Success);
    ASSERT_EQ(audio.Events().size(), 2U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Stolen);
    EXPECT_EQ(audio.Events()[0].reason, AudioEventReason2D::GroupVoiceLimit);
    EXPECT_EQ(audio.Events()[0].voice, first.voice);
    EXPECT_EQ(audio.Events()[1].type, AudioEventType2D::Started);
    EXPECT_EQ(audio.Events()[1].voice, replacement.voice);

    EXPECT_FALSE(audio.InspectVoice(first.voice).has_value());
    EXPECT_TRUE(audio.InspectVoice(second.voice).has_value());
    EXPECT_TRUE(audio.InspectVoice(replacement.voice).has_value());
    EXPECT_EQ(audio.Stop(first.voice), AudioCommandResult2D::StaleVoice);

    const AudioMetrics2D metrics = audio.Metrics();
    EXPECT_EQ(metrics.activeVoiceCount, 2U);
    EXPECT_EQ(metrics.activeVoiceCountByGroup[static_cast<std::size_t>(AudioGroup2D::Sfx)], 2U);
    EXPECT_EQ(metrics.stolenVoiceCount, 1U);
}

TEST(Audio2DVoiceLimits, GlobalStealChoosesOldestAcrossGroups)
{
    VoiceLimitFixture fixture{};
    const scene::EntityId music = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "music", AudioGroup2D::Music);
    const scene::EntityId sfx = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "sfx", AudioGroup2D::Sfx);
    const scene::EntityId ui = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "ui", AudioGroup2D::Ui);

    AudioSystem2D audio{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(audio.ReserveVoices(3U));
    ASSERT_TRUE(audio.ReserveEvents(16U));

    AudioVoiceLimits2D limits{};
    limits.globalLimit = 2U;
    limits.overflowPolicy = AudioVoiceOverflowPolicy2D::StealOldest;
    ASSERT_TRUE(audio.SetVoiceLimits(limits));

    const AudioPlayResult2D first = audio.Play(music);
    const AudioPlayResult2D second = audio.Play(sfx);
    ASSERT_EQ(first.result, AudioCommandResult2D::Success);
    ASSERT_EQ(second.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    const AudioPlayResult2D third = audio.Play(ui);
    ASSERT_EQ(third.result, AudioCommandResult2D::Success);
    ASSERT_EQ(audio.Events().size(), 2U);
    EXPECT_EQ(audio.Events()[0].type, AudioEventType2D::Stolen);
    EXPECT_EQ(audio.Events()[0].reason, AudioEventReason2D::GlobalVoiceLimit);
    EXPECT_EQ(audio.Events()[0].voice, first.voice);
    EXPECT_FALSE(audio.InspectVoice(first.voice).has_value());
    EXPECT_TRUE(audio.InspectVoice(second.voice).has_value());
    EXPECT_TRUE(audio.InspectVoice(third.voice).has_value());
}

TEST(Audio2DVoiceLimits, StealPreflightsTwoEventsAndLeavesOldVoiceUntouchedOnFailure)
{
    VoiceLimitFixture fixture{};
    const scene::EntityId firstEntity = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "first", AudioGroup2D::Sfx);
    const scene::EntityId replacementEntity = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "replacement", AudioGroup2D::Sfx);

    AudioSystem2D audio{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(audio.ReserveVoices(1U));
    ASSERT_TRUE(audio.ReserveEvents(1U));

    AudioVoiceLimits2D limits{};
    limits.overflowPolicy = AudioVoiceOverflowPolicy2D::StealOldest;
    ASSERT_TRUE(audio.SetVoiceLimits(limits));

    const AudioPlayResult2D first = audio.Play(firstEntity);
    ASSERT_EQ(first.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    const AudioPlayResult2D replacement = audio.Play(replacementEntity);
    EXPECT_EQ(replacement.result, AudioCommandResult2D::CapacityExceeded);
    EXPECT_TRUE(audio.Events().empty());
    EXPECT_TRUE(audio.InspectVoice(first.voice).has_value());

    const AudioMetrics2D metrics = audio.Metrics();
    EXPECT_EQ(metrics.activeVoiceCount, 1U);
    EXPECT_EQ(metrics.stolenVoiceCount, 0U);
    EXPECT_EQ(metrics.eventCapacityFailureCount, 1U);
}

TEST(Audio2DVoiceLimits, AutoplayBatchFailsClosedInsteadOfStealingExistingVoice)
{
    VoiceLimitFixture fixture{};
    const scene::EntityId existing = AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "existing", AudioGroup2D::Sfx);
    (void)AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "auto-sfx", AudioGroup2D::Sfx, true);
    (void)AddVoiceLimitSource(
        fixture.scene, fixture.types.source, "auto-music", AudioGroup2D::Music, true);

    AudioSystem2D audio{fixture.scene, fixture.resources, fixture.types.source};
    ASSERT_TRUE(audio.ReserveVoices(3U));
    ASSERT_TRUE(audio.ReserveEvents(16U));

    AudioVoiceLimits2D limits{};
    limits.globalLimit = 2U;
    limits.overflowPolicy = AudioVoiceOverflowPolicy2D::StealOldest;
    ASSERT_TRUE(audio.SetVoiceLimits(limits));

    const AudioPlayResult2D existingPlay = audio.Play(existing);
    ASSERT_EQ(existingPlay.result, AudioCommandResult2D::Success);
    audio.ClearEvents();

    const AudioAutoplayReport2D autoplay = audio.StartAutoplay();
    EXPECT_EQ(autoplay.result, AudioAutoplayResult2D::VoiceLimitReached);
    EXPECT_EQ(autoplay.startedVoiceCount, 0U);
    EXPECT_EQ(autoplay.requiredVoiceCapacity, 3U);
    EXPECT_TRUE(audio.Events().empty());
    EXPECT_TRUE(audio.InspectVoice(existingPlay.voice).has_value());

    const AudioMetrics2D metrics = audio.Metrics();
    EXPECT_EQ(metrics.activeVoiceCount, 1U);
    EXPECT_EQ(metrics.stolenVoiceCount, 0U);
    EXPECT_EQ(metrics.voiceLimitRejectCount, 1U);
}
} // namespace trace2d::audio
