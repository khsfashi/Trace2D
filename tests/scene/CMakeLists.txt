add_executable(trace2d_scene_tests
    SceneTests.cpp
    SceneCompositionTests.cpp
    SceneTextTests.cpp
    SceneTextCompositionTests.cpp
    SpriteTransform2DTests.cpp
)

target_link_libraries(trace2d_scene_tests
    PRIVATE
        Trace2D::Scene
        GTest::gtest_main
)

target_compile_features(trace2d_scene_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_scene_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_scene_tests)
