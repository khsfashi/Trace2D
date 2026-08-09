add_executable(trace2d_renderer_workload
    RendererWorkloadMain.cpp
)

target_link_libraries(trace2d_renderer_workload
    PRIVATE
        Trace2D::Platform
        Trace2D::Render
)

target_compile_definitions(trace2d_renderer_workload
    PRIVATE
        TRACE2D_BUILD_CONFIGURATION="$<CONFIG>"
)

target_compile_features(trace2d_renderer_workload PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_renderer_workload)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_renderer_workload_list
        COMMAND trace2d_renderer_workload --list
    )
    set_tests_properties(
        trace2d_renderer_workload_list
        PROPERTIES
            PASS_REGULAR_EXPRESSION "dense_single_texture.*alternating_two_textures.*interleaved_culling.*\\\"status\\\":\\\"ok\\\""
    )
endif()
