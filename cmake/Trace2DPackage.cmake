include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(trace2d_install_public_target target_name export_name include_directory)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Cannot export missing Trace2D target '${target_name}'.")
    endif()

    get_filename_component(_trace2d_include_directory "${include_directory}" ABSOLUTE)

    set_target_properties(${target_name}
        PROPERTIES
            EXPORT_NAME "${export_name}"
    )

    # Existing module CMake files keep their source-tree include path for the
    # target's own compilation. Replace only the consumer interface so the
    # exported target never embeds an absolute checkout path.
    set_property(TARGET ${target_name}
        PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "$<BUILD_INTERFACE:${_trace2d_include_directory}>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    install(TARGETS ${target_name}
        EXPORT Trace2DTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY "${_trace2d_include_directory}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
endfunction()

function(trace2d_configure_sdk_package)
    set(_trace2d_package_directory "${CMAKE_INSTALL_LIBDIR}/cmake/Trace2D")

    file(READ "${PROJECT_SOURCE_DIR}/vcpkg.json" _trace2d_vcpkg_manifest)
    string(JSON TRACE2D_VCPKG_BASELINE
        ERROR_VARIABLE _trace2d_vcpkg_json_error
        GET "${_trace2d_vcpkg_manifest}" "builtin-baseline"
    )
    if(NOT _trace2d_vcpkg_json_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Unable to read builtin-baseline from vcpkg.json: ${_trace2d_vcpkg_json_error}")
    endif()

    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/Trace2DConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/Trace2DConfig.cmake"
        INSTALL_DESTINATION "${_trace2d_package_directory}"
        PATH_VARS CMAKE_INSTALL_DATADIR
    )

    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/Trace2DConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
    )

    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/Trace2DSdkMetadata.json.in"
        "${PROJECT_BINARY_DIR}/trace2d.sdk.json"
        @ONLY
    )

    install(EXPORT Trace2DTargets
        FILE Trace2DTargets.cmake
        NAMESPACE Trace2D::
        DESTINATION "${_trace2d_package_directory}"
    )

    install(FILES
        "${PROJECT_BINARY_DIR}/Trace2DConfig.cmake"
        "${PROJECT_BINARY_DIR}/Trace2DConfigVersion.cmake"
        DESTINATION "${_trace2d_package_directory}"
    )

    install(FILES
        "${PROJECT_BINARY_DIR}/trace2d.sdk.json"
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Trace2D"
    )

    install(FILES
        "${PROJECT_SOURCE_DIR}/docs/EXTERNAL_PROJECT_E1.md"
        "${PROJECT_SOURCE_DIR}/docs/AGENT_PUBLIC_API.md"
        "${PROJECT_SOURCE_DIR}/docs/agent-public-api-v1.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Trace2D/docs"
    )

    install(PROGRAMS
        "${PROJECT_SOURCE_DIR}/scripts/trace2d_doctor.ps1"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Trace2D/tools"
    )
endfunction()
