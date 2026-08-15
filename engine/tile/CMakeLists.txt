find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_tile STATIC
    src/TileMap.cpp
    src/TilePresentation2D.cpp
    src/TileSemantics2D.cpp
)

add_library(Trace2D::Tile ALIAS trace2d_tile)

target_include_directories(trace2d_tile
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_tile
    PUBLIC
        Trace2D::Render
    PRIVATE
        tomlplusplus::tomlplusplus
)
target_compile_features(trace2d_tile PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_tile)
