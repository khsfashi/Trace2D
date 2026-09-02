add_library(trace2d_core STATIC
    src/FileTransaction.cpp
    src/Version.cpp
)

add_library(Trace2D::Core ALIAS trace2d_core)

target_include_directories(trace2d_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(trace2d_core PUBLIC cxx_std_20)
target_compile_definitions(trace2d_core PRIVATE TRACE2D_VERSION_STRING="${PROJECT_VERSION}")

trace2d_enable_warnings(trace2d_core)