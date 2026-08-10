add_executable(trace2d_verify
    main.cpp
)

target_link_libraries(trace2d_verify
    PRIVATE
        Trace2D::Agent
)
target_compile_features(trace2d_verify PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_verify)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_verify_fixture
        COMMAND trace2d_verify
            --spec "${CMAKE_SOURCE_DIR}/tests/data/work_spec.trace2d.toml"
            --result "${CMAKE_SOURCE_DIR}/tests/data/work_result.trace2d.toml"
            --json
    )
    set_tests_properties(
        trace2d_verify_fixture
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\\\"state\\\":\\\"review_needed\\\".*\\\"current_revision\\\":\\\"revision-2\\\".*\\\"revision_count\\\":2.*\\\"historical_failure_count\\\":1.*\\\"requires_live_truth\\\":true"
    )
endif()
