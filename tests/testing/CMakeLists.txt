add_executable(trace2d_gameplay_tests
    GameplayScenarioTests.cpp
)

target_link_libraries(trace2d_gameplay_tests
    PRIVATE
        Trace2D::Testing
        GTest::gtest_main
)

target_compile_features(trace2d_gameplay_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_gameplay_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_gameplay_tests)
