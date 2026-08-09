find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_particles STATIC
    src/ParticleDeterminism.cpp
    src/ParticleEffect.cpp
    src/ParticleReference.cpp
)

add_library(Trace2D::Particles ALIAS trace2d_particles)

target_include_directories(trace2d_particles
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_particles
    PUBLIC
        Trace2D::Scene
    PRIVATE
        tomlplusplus::tomlplusplus
)

target_compile_features(trace2d_particles PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_particles)
