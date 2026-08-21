add_executable(trace2d_profile_tests
    Profile2DTests.cpp
)

target_link_libraries(trace2d_profile_tests
    PRIVATE
        Trace2D::Profile
        GTest::gtest_main
)

target_compile_features(trace2d_profile_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_profile_tests)

include(GoogleTest)
gtest_discover_tests(
    trace2d_profile_tests
    PROPERTIES LABELS "sanitizer-headless"
)
