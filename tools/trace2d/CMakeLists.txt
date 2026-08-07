add_executable(trace2d
    main.cpp
    AgentCommands.cpp
)

target_link_libraries(trace2d
    PRIVATE
        Trace2D::Core
        Trace2D::Platform
        Trace2D::Render
        Trace2D::Runtime
        Trace2D::Scene
        Trace2D::Agent
)
target_compile_features(trace2d PRIVATE cxx_std_20)

trace2d_enable_warnings(trace2d)

if(BUILD_TESTING)
    add_test(NAME trace2d_cli_doctor COMMAND trace2d doctor --json)
    add_test(NAME trace2d_cli_headless_smoke COMMAND trace2d run --headless --frames 120 --seed 42 --json)
    set_tests_properties(trace2d_cli_headless_smoke PROPERTIES PASS_REGULAR_EXPRESSION "\\\"frame\\\":120")

    add_test(
        NAME trace2d_cli_inspect
        COMMAND trace2d inspect
            --scene "${CMAKE_SOURCE_DIR}/tests/data/inspection.trace2d.toml"
            --frames 12
            --seed 42
            --json
    )
    set_tests_properties(
        trace2d_cli_inspect
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\\\"command\\\":\\\"inspect\\\".*\\\"frame\\\":12.*\\\"id\\\":\\\"inspection_fixture\\\""
    )

    add_test(
        NAME trace2d_cli_query
        COMMAND trace2d query
            --scene "${CMAKE_SOURCE_DIR}/tests/data/inspection.trace2d.toml"
            --selector "tag:enemy"
            --frames 12
            --seed 42
            --json
    )
    set_tests_properties(
        trace2d_cli_query
        PROPERTIES
            PASS_REGULAR_EXPRESSION "\\\"command\\\":\\\"query\\\".*\\\"kind\\\":\\\"tag\\\".*\\\"match_count\\\":1.*\\\"id\\\":\\\"enemy\\\""
    )
endif()
