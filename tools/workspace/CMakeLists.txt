add_executable(trace2d_workspace
    main.cpp
)

target_link_libraries(trace2d_workspace
    PRIVATE
        Trace2D::Agent
)
target_compile_features(trace2d_workspace PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_workspace)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_workspace_fixture
        COMMAND trace2d_workspace
            --spec "${CMAKE_SOURCE_DIR}/tests/data/work_spec.trace2d.toml"
            --result "${CMAKE_SOURCE_DIR}/tests/data/work_result.trace2d.toml"
            --json
    )
    set_tests_properties(
        trace2d_workspace_fixture
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\\\"state\\\":\\\"review_needed\\\".*\\\"current_revision\\\":\\\"revision-2\\\".*\\\"review_queue\\\":\\[\\{\\\"acceptance\\\":\\\"human-review\\\""
    )

    add_test(
        NAME trace2d_workspace_feedback_packet
        COMMAND trace2d_workspace
            --spec "${CMAKE_SOURCE_DIR}/tests/data/work_spec.trace2d.toml"
            --result "${CMAKE_SOURCE_DIR}/tests/data/work_result.trace2d.toml"
            --feedback "Make the representative result feel heavier."
            --target "acceptance/human-review"
    )
    set_tests_properties(
        trace2d_workspace_feedback_packet
        PROPERTIES
            PASS_REGULAR_EXPRESSION "kind = \\"feedback\\".*revision = \\"revision-2\\".*message = \\"Make the representative result feel heavier.\\""
    )

    add_test(
        NAME trace2d_workspace_approval_packet
        COMMAND trace2d_workspace
            --spec "${CMAKE_SOURCE_DIR}/tests/data/work_spec.trace2d.toml"
            --result "${CMAKE_SOURCE_DIR}/tests/data/work_result.trace2d.toml"
            --approve "human-review"
    )
    set_tests_properties(
        trace2d_workspace_approval_packet
        PROPERTIES
            PASS_REGULAR_EXPRESSION "kind = \\"approve\\".*acceptance = \\"human-review\\".*target = \\"acceptance/human-review\\""
    )

    add_test(
        NAME trace2d_workspace_html_fixture
        COMMAND trace2d_workspace
            --spec "${CMAKE_SOURCE_DIR}/tests/data/work_spec.trace2d.toml"
            --result "${CMAKE_SOURCE_DIR}/tests/data/work_result.trace2d.toml"
            --html "${CMAKE_CURRENT_BINARY_DIR}/workspace-fixture.html"
    )
endif()
