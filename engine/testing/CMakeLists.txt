add_library(trace2d_testing STATIC
    src/GameplayScenario.cpp
)

add_library(Trace2D::Testing ALIAS trace2d_testing)

target_include_directories(trace2d_testing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_testing
    PUBLIC
        Trace2D::Agent
        Trace2D::Input
        Trace2D::Runtime
        Trace2D::Scene
)

target_compile_features(trace2d_testing PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_testing)
