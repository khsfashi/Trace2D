find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_shadercross CONFIG REQUIRED)

add_library(trace2d_render STATIC
    src/Capture.cpp
    src/RenderData.cpp
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
        SDL3_shadercross::SDL3_shadercross
)

target_compile_features(trace2d_render PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_render)
