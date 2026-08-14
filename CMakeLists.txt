cmake_minimum_required(VERSION 3.28)

project(
    Trace2D
    VERSION 0.1.0
    DESCRIPTION "A deterministic and observable C++ 2D engine built for AI coding agents."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TRACE2D_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(TRACE2D_INSTALL_SDK "Install/export the supported Trace2D C++ SDK surface." ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Trace2DWarnings)
include(CTest)

add_subdirectory(engine)

if(TRACE2D_INSTALL_SDK)
    include(Trace2DPackage)

    trace2d_install_public_target(trace2d_core Core "${CMAKE_CURRENT_SOURCE_DIR}/engine/core/include")
    trace2d_install_public_target(trace2d_input Input "${CMAKE_CURRENT_SOURCE_DIR}/engine/input/include")
    trace2d_install_public_target(trace2d_platform Platform "${CMAKE_CURRENT_SOURCE_DIR}/engine/platform/include")
    trace2d_install_public_target(trace2d_runtime Runtime "${CMAKE_CURRENT_SOURCE_DIR}/engine/runtime/include")
    trace2d_install_public_target(trace2d_scene Scene "${CMAKE_CURRENT_SOURCE_DIR}/engine/scene/include")
    trace2d_install_public_target(trace2d_particles Particles "${CMAKE_CURRENT_SOURCE_DIR}/engine/particles/include")
    trace2d_install_public_target(trace2d_assets Assets "${CMAKE_CURRENT_SOURCE_DIR}/engine/assets/include")
    trace2d_install_public_target(trace2d_render Render "${CMAKE_CURRENT_SOURCE_DIR}/engine/render/include")
    trace2d_install_public_target(trace2d_ui Ui "${CMAKE_CURRENT_SOURCE_DIR}/engine/ui/include")
    trace2d_install_public_target(trace2d_application Application "${CMAKE_CURRENT_SOURCE_DIR}/engine/application/include")
    trace2d_install_public_target(trace2d_agent Agent "${CMAKE_CURRENT_SOURCE_DIR}/engine/agent/include")
    trace2d_configure_sdk_package()

    set(CPACK_PACKAGE_NAME "Trace2D")
    set(CPACK_PACKAGE_VENDOR "Trace2D")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "trace2d-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    include(CPack)
endif()

add_subdirectory(tools)
add_subdirectory(examples)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
