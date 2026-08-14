add_library(trace2d_application STATIC
    src/Application.cpp
)

add_library(Trace2D::Application ALIAS trace2d_application)

target_include_directories(trace2d_application
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_application
    PUBLIC
        Trace2D::Input
        Trace2D::Runtime
        Trace2D::Scene
        Trace2D::Ui
)

target_compile_features(trace2d_application PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_application)
