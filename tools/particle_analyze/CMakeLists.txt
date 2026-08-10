add_executable(trace2d_particle_analyze
    ParticleAnalyzeMain.cpp
)

target_link_libraries(trace2d_particle_analyze
    PRIVATE
        Trace2D::Particles
)

target_compile_definitions(trace2d_particle_analyze
    PRIVATE
        TRACE2D_BUILD_CONFIGURATION="$<CONFIG>"
)

target_compile_features(trace2d_particle_analyze PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_particle_analyze)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_particle_analyze_help
        COMMAND trace2d_particle_analyze --help
    )
    set_tests_properties(
        trace2d_particle_analyze_help
        PROPERTIES
            PASS_REGULAR_EXPRESSION "trace2d_particle_analyze"
    )
endif()
