add_executable(trace2d_text_tests
    TextTests.cpp
    TextLayoutTests.cpp
    TextFallbackTests.cpp
)

target_link_libraries(trace2d_text_tests
    PRIVATE
        Trace2D::Text
        GTest::gtest_main
)

target_compile_definitions(trace2d_text_tests
    PRIVATE
        TRACE2D_TEXT_TEST_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/Trace2DTestFont.ttf"
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/Trace2DPrimaryTestFont.ttf"
)

target_compile_features(trace2d_text_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_text_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_text_tests)
