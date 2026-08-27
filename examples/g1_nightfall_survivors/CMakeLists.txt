cmake_minimum_required(VERSION 3.28)

set(TRACE2D_G1_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(
        Trace2DG1NightfallSurvivors
        VERSION 0.1.0
        DESCRIPTION "A closed-loop survivors-like Trace2D product dogfood game."
        LANGUAGES CXX
    )

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    find_package(Trace2D 0.1 CONFIG REQUIRED)
    set(TRACE2D_G1_STANDALONE ON)
endif()

function(trace2d_g1_enable_warnings target_name)
    if(COMMAND trace2d_enable_warnings)
        trace2d_enable_warnings(${target_name})
    endif()
endfunction()

find_path(TRACE2D_G1_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)

set(TRACE2D_G1_FETCH_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/runtime")
set(TRACE2D_G1_FETCH_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/FetchAssets.cmake")

add_custom_target(trace2d_g1_nightfall_survivors_assets
    COMMAND ${CMAKE_COMMAND}
        "-DTRACE2D_G1_RUNTIME_DIR=${TRACE2D_G1_FETCH_RUNTIME_DIR}"
        -P "${TRACE2D_G1_FETCH_SCRIPT}"
    COMMENT "Fetching pinned free/open Nightfall Survivors assets"
    VERBATIM
)

add_library(trace2d_g1_nightfall_survivors STATIC
    NightfallSurvivorsGame.cpp
    NightfallProduct.cpp
)
target_include_directories(trace2d_g1_nightfall_survivors
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    SYSTEM PRIVATE
        ${TRACE2D_G1_MINIAUDIO_INCLUDE_DIR}
)
target_link_libraries(trace2d_g1_nightfall_survivors
    PUBLIC
        Trace2D::Application
        Trace2D::Audio
)
target_compile_features(trace2d_g1_nightfall_survivors PUBLIC cxx_std_20)
trace2d_g1_enable_warnings(trace2d_g1_nightfall_survivors)

# Ordinary builds compile the complete product host/presentation without fetching runtime assets.
# This keeps Linux/static-analysis coverage on the real owner-facing code path.
add_library(trace2d_g1_nightfall_survivors_windowed_compile OBJECT
    WindowedMain.cpp
    NightfallPresentation.cpp
)
target_link_libraries(trace2d_g1_nightfall_survivors_windowed_compile
    PRIVATE
        trace2d_g1_nightfall_survivors
        Trace2D::Assets
        Trace2D::Audio
        Trace2D::Platform
        Trace2D::Render
        Trace2D::Runtime
        Trace2D::Text
)
target_compile_definitions(trace2d_g1_nightfall_survivors_windowed_compile
    PRIVATE
        TRACE2D_G1_RUNTIME_DIR="runtime"
)
target_compile_features(trace2d_g1_nightfall_survivors_windowed_compile PRIVATE cxx_std_20)
trace2d_g1_enable_warnings(trace2d_g1_nightfall_survivors_windowed_compile)

if(WIN32)
    add_executable(trace2d_g1_nightfall_survivors_windowed WIN32 EXCLUDE_FROM_ALL
        $<TARGET_OBJECTS:trace2d_g1_nightfall_survivors_windowed_compile>
    )
else()
    add_executable(trace2d_g1_nightfall_survivors_windowed EXCLUDE_FROM_ALL
        $<TARGET_OBJECTS:trace2d_g1_nightfall_survivors_windowed_compile>
    )
endif()
add_dependencies(
    trace2d_g1_nightfall_survivors_windowed
    trace2d_g1_nightfall_survivors_assets
)
target_link_libraries(trace2d_g1_nightfall_survivors_windowed
    PRIVATE
        trace2d_g1_nightfall_survivors
        Trace2D::Assets
        Trace2D::Audio
        Trace2D::Platform
        Trace2D::Render
        Trace2D::Runtime
        Trace2D::Text
)
target_compile_features(trace2d_g1_nightfall_survivors_windowed PRIVATE cxx_std_20)
trace2d_g1_enable_warnings(trace2d_g1_nightfall_survivors_windowed)

add_custom_command(
    TARGET trace2d_g1_nightfall_survivors_windowed
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:trace2d_g1_nightfall_survivors_windowed>/runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${TRACE2D_G1_FETCH_RUNTIME_DIR}"
        "$<TARGET_FILE_DIR:trace2d_g1_nightfall_survivors_windowed>/runtime"
    COMMENT "Packaging Nightfall Survivors runtime assets beside the executable"
    VERBATIM
)

if(TRACE2D_G1_STANDALONE)
    message(STATUS "Trace2D G1 Nightfall Survivors is consuming the installed Trace2D package contract.")
endif()
