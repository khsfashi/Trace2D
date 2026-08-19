find_package(box2d CONFIG REQUIRED)

add_library(trace2d_physics STATIC
    src/PhysicsComponents2D.cpp
    src/PhysicsWorld2D.cpp
)

add_library(Trace2D::Physics ALIAS trace2d_physics)

target_include_directories(trace2d_physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_physics
    PUBLIC
        Trace2D::Scene
    PRIVATE
        box2d::box2d
)

target_compile_features(trace2d_physics PUBLIC cxx_std_20)
trace2d_enable_warnings(trace2d_physics)
