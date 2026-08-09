find_package(nlohmann_json CONFIG REQUIRED)

add_library(trace2d_mcp STATIC
    src/McpServer.cpp
)

add_library(Trace2D::Mcp ALIAS trace2d_mcp)

target_include_directories(trace2d_mcp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_mcp
    PUBLIC
        Trace2D::Agent
        Trace2D::Testing
    PRIVATE
        Trace2D::Core
        nlohmann_json::nlohmann_json
)

target_compile_features(trace2d_mcp PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_mcp)
