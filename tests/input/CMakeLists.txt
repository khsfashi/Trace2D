add_executable(trace2d_input_tests
    InputTests.cpp
    ActionMapTests.cpp
    InputDeviceTests.cpp
    InputMapTests.cpp
)

target_link_libraries(trace2d_input_tests
    PRIVATE
        Trace2D::Input
        Trace2D::Runtime
        GTest::gtest_main
)

target_compile_features(trace2d_input_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_input_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_input_tests)
