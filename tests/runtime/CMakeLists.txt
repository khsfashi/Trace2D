add_executable(trace2d_runtime_tests
    FixedStepRuntimeTests.cpp
    MonotonicClockTests.cpp
)

target_link_libraries(trace2d_runtime_tests
    PRIVATE
        Trace2D::Runtime
        GTest::gtest_main
)

target_compile_features(trace2d_runtime_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_runtime_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_runtime_tests)
