add_library(trace2d_agent STATIC
    src/Inspection.cpp
    src/Query.cpp
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
)

target_compile_features(trace2d_agent PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_agent)
