add_executable(trace2d_agent_tests
    AssetCandidateSetTests.cpp
    AssetProductionFixtureTests.cpp
    AssetProductionSpecTests.cpp
    CameraInspectionTests.cpp
    InspectionTests.cpp
    ParticleAuthoringTests.cpp
    ParticleInspectionTests.cpp
    QueryTests.cpp
    SceneCompositionInspectionTests.cpp
    SpriteAnimationInspectionTests.cpp
    SpriteAuthoringTests.cpp
    UiAutomationTests.cpp
    UiAuthoredAutomationTests.cpp
    UiGameInteractionTests.cpp
    WorkResultTests.cpp
    WorkSpecTests.cpp
    WorkspaceTests.cpp
    WorkspaceInspectionTests.cpp
)

target_link_libraries(trace2d_agent_tests
    PRIVATE
        Trace2D::Agent
        Trace2D::Assets
        GTest::gtest_main
)

target_compile_definitions(trace2d_agent_tests
    PRIVATE
        TRACE2D_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_compile_features(trace2d_agent_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_agent_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_agent_tests)