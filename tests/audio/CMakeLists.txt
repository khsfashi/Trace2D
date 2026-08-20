find_package(SDL3 CONFIG REQUIRED)

add_executable(trace2d_audio_tests
    Audio2DTests.cpp
    AudioClipPreparation2DTests.cpp
    AudioOutput2DTests.cpp
    AudioSdlLifetimeTests.cpp
    AudioVoiceLimitTests.cpp
)

target_link_libraries(trace2d_audio_tests
    PRIVATE
        Trace2D::Audio
        Trace2D::Platform
        SDL3::SDL3
        GTest::gtest_main
)

target_compile_features(trace2d_audio_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_audio_tests)

include(GoogleTest)
gtest_discover_tests(
    trace2d_audio_tests
    PROPERTIES
        LABELS "sanitizer-headless"
        ENVIRONMENT "SDL_AUDIO_DRIVER=dummy;SDL_AUDIO_DUMMY_TIMESCALE=0"
)
