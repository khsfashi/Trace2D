add_executable(trace2d_core_tests
    VersionTests.cpp
)

target_link_libraries(trace2d_core_tests
    PRIVATE
        Trace2D::Core
        GTest::gtest_main
)

target_compile_features(trace2d_core_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_core_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_core_tests)
