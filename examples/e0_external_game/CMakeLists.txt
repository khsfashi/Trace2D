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

    if(NOT DEFINED Trace2D_DIR OR NOT EXISTS "${Trace2D_DIR}/Trace2DConfig.cmake")
        message(FATAL_ERROR "Installed Trace2D package config is unavailable for SDK metadata verification.")
    endif()
    file(READ "${Trace2D_SDK_METADATA}" _trace2d_sdk_metadata_json)
    string(JSON _trace2d_dependency_count
        ERROR_VARIABLE _trace2d_dependency_count_error
        LENGTH "${_trace2d_sdk_metadata_json}" "cmake_dependencies"
    )
    if(NOT _trace2d_dependency_count_error STREQUAL "NOTFOUND" OR _trace2d_dependency_count LESS 1)
        message(FATAL_ERROR "Installed Trace2D SDK metadata has no valid cmake_dependencies contract.")
    endif()

    file(READ "${Trace2D_DIR}/Trace2DConfig.cmake" _trace2d_package_config_text)
    string(REGEX MATCHALL "\n[ \t]*find_dependency\\([^\r\n]+\\)" _trace2d_find_dependency_matches "${_trace2d_package_config_text}")
    set(_trace2d_find_dependency_calls)
    foreach(_trace2d_find_dependency_match IN LISTS _trace2d_find_dependency_matches)
        string(STRIP "${_trace2d_find_dependency_match}" _trace2d_find_dependency_call)
        list(APPEND _trace2d_find_dependency_calls "${_trace2d_find_dependency_call}")
    endforeach()
    list(LENGTH _trace2d_find_dependency_calls _trace2d_find_dependency_count)
    if(NOT _trace2d_find_dependency_count EQUAL _trace2d_dependency_count)
        message(FATAL_ERROR "Installed Trace2D SDK metadata dependency count does not match Trace2DConfig.cmake.")
    endif()

    math(EXPR _trace2d_dependency_last "${_trace2d_dependency_count} - 1")
    foreach(_trace2d_dependency_index RANGE 0 ${_trace2d_dependency_last})
        string(JSON _trace2d_dependency_package
            ERROR_VARIABLE _trace2d_dependency_package_error
            GET "${_trace2d_sdk_metadata_json}" "cmake_dependencies" ${_trace2d_dependency_index} "package"
        )
        string(JSON _trace2d_dependency_find_mode
            ERROR_VARIABLE _trace2d_dependency_find_mode_error
            GET "${_trace2d_sdk_metadata_json}" "cmake_dependencies" ${_trace2d_dependency_index} "find_mode"
        )
        if(NOT _trace2d_dependency_package_error STREQUAL "NOTFOUND" OR NOT _trace2d_dependency_find_mode_error STREQUAL "NOTFOUND")
            message(FATAL_ERROR "Installed Trace2D SDK metadata contains an invalid dependency record.")
        endif()

        if(_trace2d_dependency_find_mode STREQUAL "config")
            set(_trace2d_expected_find_dependency "find_dependency(${_trace2d_dependency_package} CONFIG REQUIRED)")
        elseif(_trace2d_dependency_find_mode STREQUAL "default")
            set(_trace2d_expected_find_dependency "find_dependency(${_trace2d_dependency_package} REQUIRED)")
        else()
            message(FATAL_ERROR "Installed Trace2D SDK metadata contains unsupported find_mode '${_trace2d_dependency_find_mode}'.")
        endif()

        list(FIND _trace2d_find_dependency_calls "${_trace2d_expected_find_dependency}" _trace2d_find_dependency_index)
        if(_trace2d_find_dependency_index EQUAL -1)
            message(FATAL_ERROR "Installed Trace2D SDK metadata dependency '${_trace2d_dependency_package}' does not match Trace2DConfig.cmake.")
        endif()
    endforeach()

    string(JSON _trace2d_library_linkage
        ERROR_VARIABLE _trace2d_library_linkage_error
        GET "${_trace2d_sdk_metadata_json}" "consumer_build" "library_linkage"
    )
    string(JSON _trace2d_msvc_configuration_policy
        ERROR_VARIABLE _trace2d_msvc_configuration_policy_error
        GET "${_trace2d_sdk_metadata_json}" "consumer_build" "msvc_configuration_policy"
    )
    if(NOT _trace2d_library_linkage_error STREQUAL "NOTFOUND" OR NOT _trace2d_library_linkage STREQUAL "static")
        message(FATAL_ERROR "Installed Trace2D SDK metadata has an invalid library_linkage contract.")
    endif()
    if(NOT _trace2d_msvc_configuration_policy_error STREQUAL "NOTFOUND" OR NOT _trace2d_msvc_configuration_policy STREQUAL "match-installed-library-configuration")
        message(FATAL_ERROR "Installed Trace2D SDK metadata has an invalid MSVC configuration contract.")
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

add_executable(trace2d_e0_persistence_contract
    PersistenceContractMain.cpp
)
target_link_libraries(trace2d_e0_persistence_contract PRIVATE Trace2D::Persistence)
target_compile_features(trace2d_e0_persistence_contract PRIVATE cxx_std_20)
trace2d_e0_enable_warnings(trace2d_e0_persistence_contract)

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
    add_test(NAME trace2d_e0_persistence_contract COMMAND trace2d_e0_persistence_contract)
endif()

if(TRACE2D_E0_STANDALONE)
    message(STATUS "Trace2D external consumer is using the installed Trace2D package contract with the E2 authored-scene fixture.")
endif()
