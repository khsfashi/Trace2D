function(trace2d_p0_enable_warnings target_name)
    if(COMMAND trace2d_enable_warnings)
        trace2d_enable_warnings(${target_name})
    endif()
endfunction()

add_library(trace2d_p0_tiny_playable STATIC
    TinyPlayableGame.cpp
)
target_include_directories(trace2d_p0_tiny_playable
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(trace2d_p0_tiny_playable
    PUBLIC
        Trace2D::Application
)
target_compile_features(trace2d_p0_tiny_playable PUBLIC cxx_std_20)
trace2d_p0_enable_warnings(trace2d_p0_tiny_playable)

add_executable(trace2d_p0_tiny_playable_headless
    HeadlessMain.cpp
)
target_link_libraries(trace2d_p0_tiny_playable_headless
    PRIVATE
        trace2d_p0_tiny_playable
)
target_compile_features(trace2d_p0_tiny_playable_headless PRIVATE cxx_std_20)
trace2d_p0_enable_warnings(trace2d_p0_tiny_playable_headless)

add_executable(trace2d_p0_tiny_playable_windowed
    WindowedMain.cpp
)
target_link_libraries(trace2d_p0_tiny_playable_windowed
    PRIVATE
        trace2d_p0_tiny_playable
        Trace2D::Platform
        Trace2D::Render
)
target_compile_features(trace2d_p0_tiny_playable_windowed PRIVATE cxx_std_20)
trace2d_p0_enable_warnings(trace2d_p0_tiny_playable_windowed)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_p0_tiny_playable_headless
        COMMAND trace2d_p0_tiny_playable_headless
    )
endif()
