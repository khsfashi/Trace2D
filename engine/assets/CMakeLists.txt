find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_assets STATIC
    src/SpriteAssets.cpp
    src/SpriteExtraction.cpp
    src/SpriteProcessing.cpp
    src/TextureAssets.cpp
)
