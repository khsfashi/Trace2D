add_executable(trace2d_mcp
    main.cpp
)

target_link_libraries(trace2d_mcp
    PRIVATE
        Trace2D::Agent
        Trace2D::Mcp
        Trace2D::Testing
        Trace2D::Ui
)

target_compile_features(trace2d_mcp PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_mcp)
