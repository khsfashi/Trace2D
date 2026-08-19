add_library(trace2d_audio STATIC
    src/AudioComponents2D.cpp
    src/AudioSystem2D.cpp
)

add_library(Trace2D::Audio ALIAS trace2d_audio)

target_include_directories(trace2d_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_audio
    PUBLIC
        Trace2D::Assets
        Trace2D::Scene
)

target_compile_features(trace2d_audio PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_audio)
