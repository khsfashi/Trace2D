add_executable(trace2d_tile_tests
    GeneratedTileMap2DTests.cpp
    GeneratedTileMap2DValidationTests.cpp
    TileMapTests.cpp
    TilePresentation2DTests.cpp
    TileSemantics2DTests.cpp
    TileTerrainRules2DTests.cpp
)

target_link_libraries(trace2d_tile_tests
    PRIVATE
        Trace2D::Tile
        GTest::gtest_main
)

target_compile_features(trace2d_tile_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_tile_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_tile_tests)
