add_library(trace2d_e0_external_game STATIC
    e0_external_game/ExampleGame.cpp
)

target_include_directories(trace2d_e0_external_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/e0_external_game
)

target_link_libraries(trace2d_e0_external_game
    PUBLIC
        Trace2D::Application
        Trace2D::Agent
)

target_compile_features(trace2d_e0_external_game PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_e0_external_game)

add_executable(trace2d_e0_external_headless
    e0_external_game/HeadlessMain.cpp
)
target_link_libraries(trace2d_e0_external_headless PRIVATE trace2d_e0_external_game)
target_compile_features(trace2d_e0_external_headless PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_e0_external_headless)

add_executable(trace2d_e0_external_windowed
    e0_external_game/WindowedMain.cpp
)
target_link_libraries(trace2d_e0_external_windowed
    PRIVATE
        trace2d_e0_external_game
        Trace2D::Platform
        Trace2D::Render
)
target_compile_features(trace2d_e0_external_windowed PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_e0_external_windowed)

if(BUILD_TESTING)
    add_test(NAME trace2d_e0_external_game_headless COMMAND trace2d_e0_external_headless)
endif()
