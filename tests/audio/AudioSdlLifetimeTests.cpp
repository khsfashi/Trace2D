#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/platform/Platform.hpp>

#include <gtest/gtest.h>

namespace trace2d::audio
{
TEST(AudioOutput2D, PlatformDestructionDoesNotGloballyQuitLiveAudioSubsystem)
{
    assets::ResourceRegistry resources{"."};
    AudioOutputConfig2D config{};
    config.voiceCapacity = 1U;
    config.preloadCacheCapacity = 1U;
    config.refillChunkFrames = 4U;
    config.targetQueuedFrames = 4U;
    config.maxRefillChunksPerPump = 1U;

    AudioOutput2D output{resources, config};
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
} // namespace trace2d::audio
