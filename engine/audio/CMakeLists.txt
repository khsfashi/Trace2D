find_path(TRACE2D_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

add_library(trace2d_audio STATIC
    src/AudioClipPreparation2D.cpp
    src/AudioComponents2D.cpp
    src/AudioOutput2D.cpp
    src/AudioOutput2DPump.cpp
    src/AudioOutput2DSync.cpp
    src/AudioSystem2D.cpp
)

add_library(Trace2D::Audio ALIAS trace2d_audio)

target_include_directories(trace2d_audio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(trace2d_audio
    SYSTEM PRIVATE
        ${TRACE2D_MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(trace2d_audio
    PUBLIC
        Trace2D::Assets
        Trace2D::Scene
    PRIVATE
        SDL3::SDL3
)

target_compile_features(trace2d_audio PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_audio)
