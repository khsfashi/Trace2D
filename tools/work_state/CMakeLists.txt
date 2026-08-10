add_executable(trace2d_work_state
    main.cpp
)

target_link_libraries(trace2d_work_state
    PRIVATE
        Trace2D::Agent
)
target_compile_features(trace2d_work_state PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_work_state)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_work_state_fixture
        COMMAND trace2d_work_state
            --spec "${CMAKE_SOURCE_DIR}/tests/data/work_spec.trace2d.toml"
            --capabilities "${CMAKE_SOURCE_DIR}/config/trace2d.capabilities.toml"
            --json
    )
    set_tests_properties(
        trace2d_work_state_fixture
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\\\"local_readiness\\\":\\\"review_needed\\\".*\\\"requires_live_truth\\\":true.*\\\"eligible\\\":true"
    )
endif()
