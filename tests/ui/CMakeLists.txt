add_executable(trace2d_ui_tests
    UiTests.cpp
    UiLayoutTests.cpp
    UiStackLayoutTests.cpp
    UiContentExtentTests.cpp
    UiAuthoredScrollTests.cpp
    UiAuthoredWidgetTests.cpp
    UiPointerTests.cpp
    UiNavigationTests.cpp
    UiDirectionalNavigationTests.cpp
    UiModalScopeTests.cpp
    UiClippingTests.cpp
    UiScrollTests.cpp
    UiModalAgentTests.cpp
    UiProgressTests.cpp
    UiImageTests.cpp
    UiLimitsTests.cpp
    TextInputTests.cpp
    UiProductionTextTests.cpp
    UiPresentation2DTests.cpp
    UiPresentationMutationTests.cpp
    UiPresentationGpuSmokeTests.cpp
)

target_link_libraries(trace2d_ui_tests
    PRIVATE
        Trace2D::Ui
        Trace2D::Agent
        Trace2D::Render
        GTest::gtest_main
)

target_compile_definitions(trace2d_ui_tests
    PRIVATE
        TRACE2D_UI_TEST_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../text/fixtures/Trace2DTestFont.ttf"
)

target_compile_features(trace2d_ui_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_ui_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_ui_tests)
