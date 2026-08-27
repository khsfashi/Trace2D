cmake_minimum_required(VERSION 3.28)

set(TRACE2D_G0_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(
        Trace2DG0DungeonDash
        VERSION 0.1.0
        DESCRIPTION "A small game-first Trace2D dungeon chase using CC0 Kenney assets."
        LANGUAGES CXX
    )

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    find_package(Trace2D 0.1 CONFIG REQUIRED)
    set(TRACE2D_G0_STANDALONE ON)
endif()

function(trace2d_g0_enable_warnings target_name)
    if(COMMAND trace2d_enable_warnings)
        trace2d_enable_warnings(${target_name})
    endif()
endfunction()

add_library(trace2d_g0_dungeon_dash STATIC
    DungeonDashGame.cpp
)
target_include_directories(trace2d_g0_dungeon_dash
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(trace2d_g0_dungeon_dash
    PUBLIC
        Trace2D::Application
)
target_compile_features(trace2d_g0_dungeon_dash PUBLIC cxx_std_20)
trace2d_g0_enable_warnings(trace2d_g0_dungeon_dash)

add_executable(trace2d_g0_dungeon_dash_windowed
    WindowedMain.cpp
)
target_link_libraries(trace2d_g0_dungeon_dash_windowed
    PRIVATE
        trace2d_g0_dungeon_dash
        Trace2D::Assets
        Trace2D::Platform
        Trace2D::Render
)
target_compile_definitions(trace2d_g0_dungeon_dash_windowed
    PRIVATE
        TRACE2D_DUNGEON_DASH_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_features(trace2d_g0_dungeon_dash_windowed PRIVATE cxx_std_20)
trace2d_g0_enable_warnings(trace2d_g0_dungeon_dash_windowed)

if(TRACE2D_G0_STANDALONE)
    message(STATUS "Trace2D G0 Dungeon Dash is consuming the installed Trace2D package contract.")
endif()
