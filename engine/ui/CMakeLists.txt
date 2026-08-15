find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_ui STATIC
    src/Ui.cpp
    src/UiNavigation.cpp
    src/UiDirectionalNavigation.cpp
    src/UiModal.cpp
    src/UiLayout.cpp
    src/UiRaster.cpp
    src/UiText.cpp
    src/UiTextLayout.cpp
)

add_library(Trace2D::Ui ALIAS trace2d_ui)

target_include_directories(trace2d_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_ui
    PUBLIC
        Trace2D::Input
        Trace2D::Text
    PRIVATE
        tomlplusplus::tomlplusplus
)

target_compile_features(trace2d_ui PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_ui)
