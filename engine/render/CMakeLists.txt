find_package(SDL3 CONFIG REQUIRED)

add_library(trace2d_render STATIC
    src/Renderer.cpp
)

add_library(Trace2D::Render ALIAS trace2d_render)

target_include_directories(trace2d_render
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_render
    PUBLIC
        Trace2D::Platform
    PRIVATE
        SDL3::SDL3
)

target_compile_features(trace2d_render PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_render)
