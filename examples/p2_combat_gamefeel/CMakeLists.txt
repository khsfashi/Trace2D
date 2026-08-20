cmake_minimum_required(VERSION 3.28)

set(TRACE2D_P2_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(
        Trace2DP2CombatGameFeel
        VERSION 0.1.0
        DESCRIPTION "Bounded external Physics2D + Audio combat product proof."
        LANGUAGES CXX
    )

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
    find_package(Trace2D 0.1 CONFIG REQUIRED)
    set(TRACE2D_P2_STANDALONE ON)
endif()

function(trace2d_p2_enable_warnings target_name)
    if(COMMAND trace2d_enable_warnings)
        trace2d_enable_warnings(${target_name})
    endif()
endfunction()

set(TRACE2D_P2_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/runtime")

add_library(trace2d_p2_combat_gamefeel STATIC
    CombatGame.cpp
)
target_include_directories(trace2d_p2_combat_gamefeel
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(trace2d_p2_combat_gamefeel
    PUBLIC
        Trace2D::Application
        Trace2D::Audio
        Trace2D::Physics
)
target_compile_features(trace2d_p2_combat_gamefeel PUBLIC cxx_std_20)
trace2d_p2_enable_warnings(trace2d_p2_combat_gamefeel)

add_executable(trace2d_p2_combat_gamefeel_headless
    HeadlessMain.cpp
)
target_link_libraries(trace2d_p2_combat_gamefeel_headless
    PRIVATE
        trace2d_p2_combat_gamefeel
)
target_compile_definitions(trace2d_p2_combat_gamefeel_headless
    PRIVATE
        TRACE2D_P2_RUNTIME_DIR="${TRACE2D_P2_RUNTIME_DIR}"
)
target_compile_features(trace2d_p2_combat_gamefeel_headless PRIVATE cxx_std_20)
trace2d_p2_enable_warnings(trace2d_p2_combat_gamefeel_headless)

add_executable(trace2d_p2_combat_gamefeel_windowed
    WindowedMain.cpp
)
target_link_libraries(trace2d_p2_combat_gamefeel_windowed
    PRIVATE
        trace2d_p2_combat_gamefeel
        Trace2D::Platform
        Trace2D::Render
)
target_compile_definitions(trace2d_p2_combat_gamefeel_windowed
    PRIVATE
        TRACE2D_P2_RUNTIME_DIR="${TRACE2D_P2_RUNTIME_DIR}"
)
target_compile_features(trace2d_p2_combat_gamefeel_windowed PRIVATE cxx_std_20)
trace2d_p2_enable_warnings(trace2d_p2_combat_gamefeel_windowed)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_p2_combat_gamefeel_headless
        COMMAND trace2d_p2_combat_gamefeel_headless
    )
endif()

if(TRACE2D_P2_STANDALONE)
    message(STATUS "Trace2D P2 combat game-feel proof is consuming the installed Trace2D package contract.")
endif()
