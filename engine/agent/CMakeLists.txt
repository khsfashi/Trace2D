find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_agent STATIC
    src/Inspection.cpp
    src/ParticleInspection.cpp
    src/Query.cpp
    src/UiAutomation.cpp
    src/WorkResult.cpp
    src/WorkSpec.cpp
    src/Workspace.cpp
)

add_library(Trace2D::Agent ALIAS trace2d_agent)

target_include_directories(trace2d_agent
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_agent
    PUBLIC
        Trace2D::Runtime
        Trace2D::Scene
        Trace2D::Ui
        Trace2D::Particles
    PRIVATE
        tomlplusplus::tomlplusplus
)

target_compile_features(trace2d_agent PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_agent)