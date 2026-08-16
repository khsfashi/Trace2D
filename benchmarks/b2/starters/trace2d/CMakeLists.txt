cmake_minimum_required(VERSION 3.28)

project(
    Trace2DB2ScoredStarter
    VERSION 0.1.0
    DESCRIPTION "Neutral external Trace2D starter for Benchmark B2."
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Trace2D 0.1 CONFIG REQUIRED)

add_executable(trace2d_b2_scored_starter
    main.cpp
)

target_link_libraries(trace2d_b2_scored_starter
    PRIVATE
        Trace2D::Application
        Trace2D::Agent
        Trace2D::Platform
        Trace2D::Render
)

target_compile_features(trace2d_b2_scored_starter PRIVATE cxx_std_20)
