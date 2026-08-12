add_library(trace2d_runtime STATIC
    src/FixedStepRuntime.cpp
    src/MonotonicClock.cpp
    src/SpriteAnimator2D.cpp
)

add_library(Trace2D::Runtime ALIAS trace2d_runtime)

target_include_directories(trace2d_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_runtime PUBLIC Trace2D::Core)
target_compile_features(trace2d_runtime PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_runtime)
