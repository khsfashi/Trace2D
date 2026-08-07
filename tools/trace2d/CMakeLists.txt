add_executable(trace2d
    main.cpp
)

target_link_libraries(trace2d PRIVATE Trace2D::Core)
target_compile_features(trace2d PRIVATE cxx_std_20)

trace2d_enable_warnings(trace2d)
