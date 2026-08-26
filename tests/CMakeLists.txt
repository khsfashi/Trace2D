if(TRACE2D_INSTALL_SDK)
    add_test(
        NAME trace2d_package_agent_index_failure_path_contract
        COMMAND ${CMAKE_COMMAND}
            "-DTRACE2D_PACKAGE_CONFIG=${PROJECT_BINARY_DIR}/Trace2DConfig.cmake"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/PackageConfigAgentIndexContract.cmake"
    )
    set_tests_properties(
        trace2d_package_agent_index_failure_path_contract
        PROPERTIES LABELS "sanitizer-headless"
    )
endif()

find_package(GTest CONFIG REQUIRED)

add_subdirectory(core)
add_subdirectory(profile)
add_subdirectory(input)
add_subdirectory(platform)
add_subdirectory(render)
add_subdirectory(runtime)
add_subdirectory(scene)
add_subdirectory(physics)
add_subdirectory(ui)
add_subdirectory(application)
add_subdirectory(persistence)
add_subdirectory(agent)
add_subdirectory(testing)
add_subdirectory(mcp)
add_subdirectory(assets)
add_subdirectory(audio)
add_subdirectory(particles)
add_subdirectory(tile)
add_subdirectory(text)
add_subdirectory(e2e)
add_subdirectory(benchmark)
