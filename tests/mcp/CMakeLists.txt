find_package(nlohmann_json CONFIG REQUIRED)

add_executable(trace2d_mcp_tests
    McpServerTests.cpp
    SpriteAnimationMcpTests.cpp
)

target_link_libraries(trace2d_mcp_tests
    PRIVATE
        Trace2D::Assets
        Trace2D::Mcp
        Trace2D::Ui
        nlohmann_json::nlohmann_json
        GTest::gtest_main
)

target_compile_features(trace2d_mcp_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_mcp_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_mcp_tests)
