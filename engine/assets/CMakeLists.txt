find_path(STB_IMAGE_INCLUDE_DIRS "stb_image.h" REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_assets STATIC
    src/ResourceRegistry.cpp
    src/ResourceRegistryLookup.cpp
    src/SpriteAssets.cpp
    src/SpriteExtraction.cpp
    src/SpriteGeneration.cpp
    src/SpriteGeneratorInterop.cpp
    src/SpriteImport.cpp
    src/SpriteProcessing.cpp
    src/SpriteQuality.cpp
    src/TextureAssets.cpp
)

add_library(Trace2D::Assets ALIAS trace2d_assets)

target_include_directories(trace2d_assets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${STB_IMAGE_INCLUDE_DIRS}
)

target_link_libraries(trace2d_assets
    PRIVATE
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
)

target_compile_features(trace2d_assets PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_assets)