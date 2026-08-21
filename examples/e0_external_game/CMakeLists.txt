cmake_minimum_required(VERSION 3.28)

set(TRACE2D_E0_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(
        Trace2DE0ExternalGame
        VERSION 0.1.0
        DESCRIPTION "Minimal external Trace2D game consumer."
        LANGUAGES CXX
    )

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    include(CTest)
    find_package(Trace2D 0.1 CONFIG REQUIRED)
    set(TRACE2D_E0_STANDALONE ON)
endif()

function(trace2d_e0_enable_warnings target_name)
    if(COMMAND trace2d_enable_warnings)
        trace2d_enable_warnings(${target_name})
    endif()
endfunction()

add_library(trace2d_e0_external_game STATIC
    ExampleGame.cpp
)

target_include_directories(trace2d_e0_external_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(trace2d_e0_external_game
    PUBLIC
        Trace2D::Application
        Trace2D::Agent
        Trace2D::Profile
)

target_compile_features(trace2d_e0_external_game PUBLIC cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_external_game)

add_executable(trace2d_e0_external_headless
    HeadlessMain.cpp
)
target_link_libraries(trace2d_e0_external_headless PRIVATE trace2d_e0_external_game)
target_compile_features(trace2d_e0_external_headless PRIVATE cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_external_headless)

add_executable(trace2d_e0_external_windowed
    WindowedMain.cpp
)
target_link_libraries(trace2d_e0_external_windowed
    PRIVATE
        trace2d_e0_external_game
        Trace2D::Platform
        Trace2D::Render
)
target_compile_features(trace2d_e0_external_windowed PRIVATE cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_external_windowed)

if(BUILD_TESTING)
    add_test(NAME trace2d_e0_external_game_headless COMMAND trace2d_e0_external_headless)
    set_tests_properties(trace2d_e0_external_game_headless PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(TRACE2D_E0_STANDALONE)
    message(STATUS "Trace2D external consumer is using the installed Trace2D package contract with the E2 authored-scene fixture.")
endif()
