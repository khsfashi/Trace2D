#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/platform/Platform.hpp>

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <cstddef>

namespace trace2d::audio
{
namespace
{
[[nodiscard]] AudioOutputConfig2D SmallLifetimeConfig()
{
    AudioOutputConfig2D config{};
    config.voiceCapacity = 1U;
    config.preloadCacheCapacity = 1U;
    config.refillChunkFrames = 4U;
    config.targetQueuedFrames = 4U;
    config.maxRefillChunksPerPump = 1U;
    return config;
}
}

TEST(AudioOutput2D, PlatformDestructionDoesNotGloballyQuitLiveAudioSubsystem)
{
    assets::ResourceRegistry resources{"."};
    AudioOutput2D output{resources, SmallLifetimeConfig()};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();

    {
        platform::PlatformConfig platformConfig{};
        platformConfig.mode = platform::StartupMode::Headless;
        platform::Platform platform{platformConfig};
        EXPECT_EQ(platform.Mode(), platform::StartupMode::Headless);
    }

    ASSERT_EQ(output.Suspend(), AudioOutputResult2D::Success) << output.LastDiagnostic();
    ASSERT_EQ(output.Resume(), AudioOutputResult2D::Success) << output.LastDiagnostic();
    EXPECT_EQ(output.State(), AudioOutputState2D::Running);
}

TEST(AudioOutput2D, UnrelatedLogicalPlaybackLossDoesNotForceRecoveryAfterPlatformPolling)
{
    assets::ResourceRegistry resources{"."};
    AudioOutput2D output{resources, SmallLifetimeConfig()};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Headless;
    platform::Platform platform{platformConfig};

    const SDL_AudioDeviceID unrelatedDevice =
        SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    ASSERT_NE(unrelatedDevice, 0U) << SDL_GetError();

    SDL_FlushEvent(SDL_EVENT_AUDIO_DEVICE_REMOVED);
    SDL_Event event{};
    event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
    event.adevice.which = unrelatedDevice;
    event.adevice.recording = false;
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();

    platform::PlatformEvent ignored{};
    std::size_t pollCount = 0U;
    while (SDL_HasEvent(SDL_EVENT_AUDIO_DEVICE_REMOVED) && pollCount < 32U)
    {
        ASSERT_TRUE(platform.PollEvent(ignored));
        ++pollCount;
    }
    ASSERT_FALSE(SDL_HasEvent(SDL_EVENT_AUDIO_DEVICE_REMOVED));

    const AudioOutputDeviceEventReport2D report = output.PollDeviceEvents();
    EXPECT_EQ(report.result, AudioOutputResult2D::Success);
    EXPECT_EQ(report.processedEventCount, 0U);
    EXPECT_FALSE(report.recoveryRequested);
    EXPECT_EQ(output.State(), AudioOutputState2D::Running);
    EXPECT_EQ(output.Metrics().deviceLossEventCount, 0U);

    SDL_CloseAudioDevice(unrelatedDevice);
}

TEST(AudioOutput2D, UnrelatedLogicalPlaybackFormatChangeDoesNotForceRecovery)
{
    assets::ResourceRegistry resources{"."};
    AudioOutput2D output{resources, SmallLifetimeConfig()};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();

    const SDL_AudioDeviceID unrelatedDevice =
        SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    ASSERT_NE(unrelatedDevice, 0U) << SDL_GetError();

    SDL_FlushEvent(SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED);
    SDL_Event event{};
    event.type = SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED;
    event.adevice.which = unrelatedDevice;
    event.adevice.recording = false;
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();

    const AudioOutputDeviceEventReport2D report = output.PollDeviceEvents();
    EXPECT_EQ(report.result, AudioOutputResult2D::Success);
    EXPECT_EQ(report.processedEventCount, 0U);
    EXPECT_FALSE(report.recoveryRequested);
    EXPECT_EQ(output.State(), AudioOutputState2D::Running);
    EXPECT_EQ(output.Metrics().deviceFormatChangeEventCount, 0U);

    SDL_CloseAudioDevice(unrelatedDevice);
}
} // namespace trace2d::audio
