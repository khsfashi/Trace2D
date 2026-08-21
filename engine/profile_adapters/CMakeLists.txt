add_library(trace2d_profile_adapters STATIC
    src/ProfileAdapters2D.cpp
)

add_library(Trace2D::ProfileAdapters ALIAS trace2d_profile_adapters)

target_include_directories(trace2d_profile_adapters
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_profile_adapters
    PUBLIC
        Trace2D::Profile
        Trace2D::Assets
        Trace2D::Audio
        Trace2D::Particles
        Trace2D::Physics
        Trace2D::Render
)

target_compile_features(trace2d_profile_adapters PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_profile_adapters)
