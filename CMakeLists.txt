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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Trace2DWarnings)
include(CTest)

add_subdirectory(engine)
add_subdirectory(tools)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
