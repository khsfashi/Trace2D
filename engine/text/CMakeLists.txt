find_package(Freetype REQUIRED)

add_library(trace2d_text STATIC
    src/Text.cpp
    src/TextLayout.cpp
)

add_library(Trace2D::Text ALIAS trace2d_text)

target_include_directories(trace2d_text
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_text
    PUBLIC
        Trace2D::Assets
    PRIVATE
        Freetype::Freetype
)

target_compile_features(trace2d_text PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_text)
