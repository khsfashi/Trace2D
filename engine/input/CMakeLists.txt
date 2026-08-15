find_package(tomlplusplus CONFIG REQUIRED)

add_library(trace2d_input STATIC
    src/Input.cpp
    src/ActionMap.cpp
    src/InputMap.cpp
)

add_library(Trace2D::Input ALIAS trace2d_input)

target_include_directories(trace2d_input
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_input
    PRIVATE
        tomlplusplus::tomlplusplus
)
target_compile_features(trace2d_input PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_input)
