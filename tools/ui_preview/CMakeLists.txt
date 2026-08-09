add_executable(trace2d_ui_preview
    UiPreview.cpp
)

target_link_libraries(trace2d_ui_preview
    PRIVATE
        Trace2D::Platform
        Trace2D::Render
        Trace2D::Ui
)

target_compile_features(trace2d_ui_preview PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_ui_preview)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_ui_preview_headless
        COMMAND trace2d_ui_preview
            --headless
            --ui "${CMAKE_SOURCE_DIR}/samples/ui/basic_ui.trace2d.toml"
            --json
    )
    set_tests_properties(
        trace2d_ui_preview_headless
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\\\"command\\\":\\\"ui-preview\\\".*\\\"elements\\\":4.*\\\"glyphs\\\":24.*\\\"status\\\":\\\"ok\\\""
    )
endif()
