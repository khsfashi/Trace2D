find_package(SDL3 CONFIG REQUIRED)

add_library(trace2d_platform STATIC
    src/Platform.cpp
)

add_library(Trace2D::Platform ALIAS trace2d_platform)

target_include_directories(trace2d_platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_platform
    PUBLIC
        Trace2D::Input
    PRIVATE
        SDL3::SDL3
)

target_compile_features(trace2d_platform PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_platform)
