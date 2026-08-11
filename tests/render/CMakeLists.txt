add_executable(trace2d_render_tests
    CaptureTests.cpp
    ParticleGpuConformanceTests.cpp
    ParticleGpuRuntimeTests.cpp
    ParticleGpuSmokeTests.cpp
    RenderDataTests.cpp
    RendererTests.cpp
    RendererWorkloadTests.cpp
    SpriteAppearance2DTests.cpp
    SpriteGeometry2DTests.cpp
    SpriteGpuSmokeTests.cpp
    SpritePresentation2DTests.cpp
    SpriteRenderContractTests.cpp
)

target_link_libraries(trace2d_render_tests
    PRIVATE
        Trace2D::Particles
        Trace2D::Render
        GTest::gtest_main
)

target_compile_features(trace2d_render_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_render_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_render_tests)