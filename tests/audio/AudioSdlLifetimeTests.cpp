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

TEST(AudioOutput2D, PlatformPollingDoesNotStealPlaybackDeviceRecoverySignal)
{
    assets::ResourceRegistry resources{"."};
    AudioOutput2D output{resources, SmallLifetimeConfig()};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Headless;
    platform::Platform platform{platformConfig};

    SDL_FlushEvent(SDL_EVENT_AUDIO_DEVICE_REMOVED);
    SDL_Event event{};
    event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
    event.adevice.which = 1U;
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
    EXPECT_EQ(report.processedEventCount, 1U);
    EXPECT_TRUE(report.recoveryRequested);
    EXPECT_EQ(output.State(), AudioOutputState2D::RecoveryPending);
    EXPECT_EQ(output.Metrics().deviceLossEventCount, 1U);
}

TEST(AudioOutput2D, PlaybackFormatChangeRequestsRecovery)
{
    assets::ResourceRegistry resources{"."};
    AudioOutput2D output{resources, SmallLifetimeConfig()};
    ASSERT_EQ(output.Start(), AudioOutputResult2D::Success) << output.LastDiagnostic();

    SDL_FlushEvent(SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED);
    SDL_Event event{};
    event.type = SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED;
    event.adevice.which = 1U;
    event.adevice.recording = false;
    ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();

    const AudioOutputDeviceEventReport2D report = output.PollDeviceEvents();
    EXPECT_EQ(report.result, AudioOutputResult2D::Success);
    EXPECT_EQ(report.processedEventCount, 1U);
    EXPECT_TRUE(report.recoveryRequested);
    EXPECT_EQ(output.State(), AudioOutputState2D::RecoveryPending);
    EXPECT_EQ(output.Metrics().deviceFormatChangeEventCount, 1U);
}
} // namespace trace2d::audio
