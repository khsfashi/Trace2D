find_package(nlohmann_json CONFIG REQUIRED)

add_library(trace2d_profile STATIC
    src/Profile2D.cpp
    src/ProfileReport2D.cpp
    src/StructuralProfile2D.cpp
)

add_library(Trace2D::Profile ALIAS trace2d_profile)

target_include_directories(trace2d_profile
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(trace2d_profile
    PUBLIC
        Trace2D::Core
    PRIVATE
        nlohmann_json::nlohmann_json
)
target_compile_features(trace2d_profile PUBLIC cxx_std_20)

trace2d_enable_warnings(trace2d_profile)
