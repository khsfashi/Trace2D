find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_scene STATIC
    src/Camera2D.cpp
    src/CameraSelection2D.cpp
    src/Components.cpp
    src/Scene.cpp
    src/SceneText.cpp
    src/SpriteTransform2D.cpp
    src/TweenBinding2DControl.cpp
    src/TweenBinding2DStep.cpp
    src/TweenBinding2DStorage.cpp
    src/World.cpp
)

add_library(Trace2D::Scene ALIAS trace2d_scene)

target_include_directories(trace2d_scene
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_scene
    PUBLIC
        Trace2D::Assets
        Trace2D::Core
        Trace2D::Runtime
    PRIVATE
        tomlplusplus::tomlplusplus
)

target_compile_features(trace2d_scene PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_scene)
