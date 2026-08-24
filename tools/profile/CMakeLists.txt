add_executable(trace2d_profile_cli
    main.cpp
)

set_target_properties(trace2d_profile_cli PROPERTIES OUTPUT_NAME "trace2d-profile")

target_link_libraries(trace2d_profile_cli
    PRIVATE
        Trace2D::Core
        Trace2D::Assets
        Trace2D::Particles
        Trace2D::Platform
        Trace2D::Profile
        Trace2D::ProfileAdapters
        Trace2D::Render
        Trace2D::Runtime
)
target_compile_features(trace2d_profile_cli PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_profile_cli)

set(TRACE2D_PROFILE_SOURCE_REVISION "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE TRACE2D_PROFILE_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE TRACE2D_PROFILE_GIT_RESULT
    )
    if(TRACE2D_PROFILE_GIT_RESULT EQUAL 0 AND TRACE2D_PROFILE_GIT_REVISION MATCHES "^[0-9a-fA-F]+$")
        set(TRACE2D_PROFILE_SOURCE_REVISION "${TRACE2D_PROFILE_GIT_REVISION}")
    endif()
endif()

target_compile_definitions(
    trace2d_profile_cli
    PRIVATE TRACE2D_PROFILE_SOURCE_REVISION="${TRACE2D_PROFILE_SOURCE_REVISION}"
)

if(BUILD_TESTING)
    add_test(
        NAME trace2d_profile_cli_headless_contract
        COMMAND ${CMAKE_COMMAND}
            "-DTRACE2D_PROFILE_CLI=$<TARGET_FILE:trace2d_profile_cli>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ProfileCliTest.cmake"
    )
    set_tests_properties(
        trace2d_profile_cli_headless_contract
        PROPERTIES LABELS "sanitizer-headless"
    )
endif()
