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

    add_test(
        NAME trace2d_particle_analyze_structure
        COMMAND trace2d_particle_analyze
            --project-root "${CMAKE_SOURCE_DIR}"
            --effect "tests/particles/fixtures/analyze.trace2d.particle.toml"
            --frames 4
            --seed 7
            --stable-id 11
    )
    set_tests_properties(
        trace2d_particle_analyze_structure
        PROPERTIES
            PASS_REGULAR_EXPRESSION "deterministic_structure.*backend_changed_by_analyzer.*false"
    )

    foreach(workload IN ITEMS
        workload_cpu_small
        workload_cpu_medium
        workload_gpu_scale
    )
        add_test(
            NAME trace2d_particle_analyze_${workload}
            COMMAND trace2d_particle_analyze
                --project-root "${CMAKE_SOURCE_DIR}"
                --effect "tests/particles/fixtures/${workload}.trace2d.particle.toml"
                --frames 240
                --seed 1311768467463790320
                --stable-id 1234605616436508552
        )
        set_tests_properties(
            trace2d_particle_analyze_${workload}
            PROPERTIES
                PASS_REGULAR_EXPRESSION "deterministic_structure.*backend_changed_by_analyzer.*false"
        )
    endforeach()

    # This validates timing-output provenance only. It deliberately has no timing threshold.
    add_test(
        NAME trace2d_particle_analyze_timing_schema
        COMMAND trace2d_particle_analyze
            --project-root "${CMAKE_SOURCE_DIR}"
            --effect "tests/particles/fixtures/analyze.trace2d.particle.toml"
            --frames 1
            --timing
            --warmup 0
            --iterations 1
            --machine-label "ci-schema-smoke"
            --cpu-model "unscored-ci-cpu"
    )
    set_tests_properties(
        trace2d_particle_analyze_timing_schema
        PROPERTIES
            PASS_REGULAR_EXPRESSION "machine_dependent_timing.*deterministic_equality_member.*false"
    )
endif()