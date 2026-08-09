add_executable(trace2d_mcp_host
    main.cpp
)

set_target_properties(trace2d_mcp_host PROPERTIES
    OUTPUT_NAME trace2d_mcp
)

target_link_libraries(trace2d_mcp_host
    PRIVATE
        Trace2D::Agent
        Trace2D::Mcp
        Trace2D::Testing
        Trace2D::Ui
)

target_compile_features(trace2d_mcp_host PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_mcp_host)
