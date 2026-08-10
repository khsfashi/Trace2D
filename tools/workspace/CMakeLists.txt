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
    set(TRACE2D_WORKSPACE_SPEC "${CMAKE_SOURCE_DIR}/tests/data/workspace_spec.trace2d.toml")
    set(TRACE2D_WORKSPACE_RESULT "${CMAKE_SOURCE_DIR}/tests/data/workspace_result.trace2d.toml")

    add_test(
        NAME trace2d_workspace_fixture
        COMMAND trace2d_workspace
            --spec "${TRACE2D_WORKSPACE_SPEC}"
            --result "${TRACE2D_WORKSPACE_RESULT}"
            --json
    )
    set_tests_properties(
        trace2d_workspace_fixture
        PROPERTIES
            PASS_REGULAR_EXPRESSION "review_needed.*revision-2.*feel-review"
    )

    add_test(
        NAME trace2d_workspace_feedback_packet
        COMMAND trace2d_workspace
            --spec "${TRACE2D_WORKSPACE_SPEC}"
            --result "${TRACE2D_WORKSPACE_RESULT}"
            --feedback "Make the revised hit even heavier."
            --target "effect/hit"
            --acceptance "feel-review"
    )
    set_tests_properties(
        trace2d_workspace_feedback_packet
        PROPERTIES
            PASS_REGULAR_EXPRESSION "feedback.*revision-2.*feel-review.*Make the revised hit even heavier"
    )

    add_test(
        NAME trace2d_workspace_approval_packet
        COMMAND trace2d_workspace
            --spec "${TRACE2D_WORKSPACE_SPEC}"
            --result "${TRACE2D_WORKSPACE_RESULT}"
            --approve "feel-review"
    )
    set_tests_properties(
        trace2d_workspace_approval_packet
        PROPERTIES
            PASS_REGULAR_EXPRESSION "approve.*feel-review.*acceptance/feel-review"
    )

    add_test(
        NAME trace2d_workspace_html_fixture
        COMMAND trace2d_workspace
            --spec "${TRACE2D_WORKSPACE_SPEC}"
            --result "${TRACE2D_WORKSPACE_RESULT}"
            --html "${CMAKE_CURRENT_BINARY_DIR}/workspace-fixture.html"
    )
endif()