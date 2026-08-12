add_executable(trace2d_sprite_animation_workload
    SpriteAnimationWorkloadMain.cpp
)

target_link_libraries(trace2d_sprite_animation_workload
    PRIVATE
        Trace2D::Assets
        Trace2D::Runtime
)

target_compile_definitions(trace2d_sprite_animation_workload
    PRIVATE
        TRACE2D_BUILD_CONFIGURATION="$<CONFIG>"
)

target_compile_features(trace2d_sprite_animation_workload PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_sprite_animation_workload)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_sprite_animation_workload_list
        COMMAND trace2d_sprite_animation_workload --list
    )
    set_tests_properties(
        trace2d_sprite_animation_workload_list
        PROPERTIES
            PASS_REGULAR_EXPRESSION "steady_loop_rational.*dense_event_ping_pong.*large_step_multi_wrap.*\"status\":\"ok\""
    )

    foreach(workload IN ITEMS steady_loop_rational dense_event_ping_pong large_step_multi_wrap)
        add_test(
            NAME trace2d_sprite_animation_workload_${workload}
            COMMAND trace2d_sprite_animation_workload --workload ${workload}
        )
        set_tests_properties(
            trace2d_sprite_animation_workload_${workload}
            PROPERTIES
                PASS_REGULAR_EXPRESSION "\"deterministic_replay\":true"
        )
    endforeach()
endif()
