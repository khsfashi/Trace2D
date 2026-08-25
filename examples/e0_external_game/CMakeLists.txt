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
    if(NOT DEFINED Trace2D_SDK_ROOT_LOCATOR OR NOT EXISTS "${Trace2D_SDK_ROOT_LOCATOR}")
        message(FATAL_ERROR "Installed Trace2D package did not expose a valid Trace2D_SDK_ROOT_LOCATOR.")
    endif()
    if(NOT DEFINED Trace2D_SDK_METADATA OR NOT EXISTS "${Trace2D_SDK_METADATA}")
        message(FATAL_ERROR "Installed Trace2D package did not expose a valid Trace2D_SDK_METADATA.")
    endif()

    file(READ "${Trace2D_SDK_ROOT_LOCATOR}" _trace2d_sdk_locator_json)
    string(JSON _trace2d_locator_kind
        ERROR_VARIABLE _trace2d_locator_kind_error
        GET "${_trace2d_sdk_locator_json}" "artifact_kind"
    )
    if(NOT _trace2d_locator_kind_error STREQUAL "NOTFOUND" OR NOT _trace2d_locator_kind STREQUAL "trace2d-sdk-root-locator")
        message(FATAL_ERROR "Installed Trace2D SDK root locator has an invalid artifact_kind.")
    endif()
    string(JSON _trace2d_locator_metadata
        ERROR_VARIABLE _trace2d_locator_metadata_error
        GET "${_trace2d_sdk_locator_json}" "sdk_metadata"
    )
    if(NOT _trace2d_locator_metadata_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Installed Trace2D SDK root locator has no valid sdk_metadata path.")
    endif()

    get_filename_component(_trace2d_sdk_root "${Trace2D_SDK_ROOT_LOCATOR}" DIRECTORY)
    cmake_path(APPEND _trace2d_sdk_root "${_trace2d_locator_metadata}" OUTPUT_VARIABLE _trace2d_locator_metadata_path)
    cmake_path(NORMAL_PATH _trace2d_locator_metadata_path)
    cmake_path(NORMAL_PATH Trace2D_SDK_METADATA OUTPUT_VARIABLE _trace2d_package_metadata_path)
    if(NOT _trace2d_locator_metadata_path STREQUAL _trace2d_package_metadata_path)
        message(FATAL_ERROR "Installed Trace2D SDK root locator does not resolve to Trace2D_SDK_METADATA.")
    endif()

    if(NOT DEFINED Trace2D_PUBLIC_API_INDEX OR NOT EXISTS "${Trace2D_PUBLIC_API_INDEX}")
        message(FATAL_ERROR "Installed Trace2D package did not expose a valid Trace2D_PUBLIC_API_INDEX.")
    endif()
    if(NOT DEFINED Trace2D_PUBLIC_API_GUIDE OR NOT EXISTS "${Trace2D_PUBLIC_API_GUIDE}")
        message(FATAL_ERROR "Installed Trace2D package did not expose a valid Trace2D_PUBLIC_API_GUIDE.")
    endif()
    if(NOT DEFINED Trace2D_EXTERNAL_PROJECT_GUIDE OR NOT EXISTS "${Trace2D_EXTERNAL_PROJECT_GUIDE}")
        message(FATAL_ERROR "Installed Trace2D package did not expose a valid Trace2D_EXTERNAL_PROJECT_GUIDE.")
    endif()
    if(NOT DEFINED Trace2D_CANONICAL_EXTERNAL_EXAMPLE_ROOT OR NOT EXISTS "${Trace2D_CANONICAL_EXTERNAL_EXAMPLE_ROOT}/ExampleGame.hpp")
        message(FATAL_ERROR "Installed Trace2D package did not expose a valid Trace2D_CANONICAL_EXTERNAL_EXAMPLE_ROOT.")
    endif()
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
        Trace2D::ProfileAdapters
)

target_compile_features(trace2d_e0_external_game PUBLIC cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_external_game)

add_executable(trace2d_e0_external_headless
    HeadlessMain.cpp
)
target_link_libraries(trace2d_e0_external_headless PRIVATE trace2d_e0_external_game)
target_compile_features(trace2d_e0_external_headless PRIVATE cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_external_headless)

add_executable(trace2d_e0_profile_contract
    ProfileContractMain.cpp
)
target_link_libraries(trace2d_e0_profile_contract PRIVATE Trace2D::Profile)
target_compile_features(trace2d_e0_profile_contract PRIVATE cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_profile_contract)

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
    add_test(NAME trace2d_e0_profile_contract COMMAND trace2d_e0_profile_contract)
endif()

if(TRACE2D_E0_STANDALONE)
    message(STATUS "Trace2D external consumer is using the installed Trace2D package contract with the E2 authored-scene fixture.")
endif()
