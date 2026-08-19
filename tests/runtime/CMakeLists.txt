add_executable(trace2d_runtime_tests
    FixedStepRuntimeTests.cpp
    MonotonicClockTests.cpp
    SpriteAnimator2DTests.cpp
    SpriteAnimatorPlaybackSA2Tests.cpp
    SpriteAnimationConformanceSA4Tests.cpp
    reference/SpriteAnimatorReferenceModel.cpp
    SpriteAnimatorReferenceOracleTests.cpp
    Tween2DTests.cpp
    Tween2DControlTests.cpp
)

target_link_libraries(trace2d_runtime_tests
    PRIVATE
        Trace2D::Assets
        Trace2D::Runtime
        GTest::gtest_main
)

target_compile_features(trace2d_runtime_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_runtime_tests)

include(GoogleTest)
gtest_discover_tests(
    trace2d_runtime_tests
    PROPERTIES LABELS "sanitizer-headless"
)
