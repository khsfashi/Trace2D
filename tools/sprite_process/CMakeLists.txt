add_executable(trace2d_sprite_process
    SpriteProcessMain.cpp
)

target_link_libraries(trace2d_sprite_process
    PRIVATE
        Trace2D::Assets
)

target_compile_features(trace2d_sprite_process PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_sprite_process)

if(BUILD_TESTING)
    add_test(
        NAME SpriteProcessingTool.Help
        COMMAND trace2d_sprite_process --help
    )
endif()
