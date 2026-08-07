add_executable(trace2d
    main.cpp
)

target_link_libraries(trace2d
    PRIVATE
        Trace2D::Core
        Trace2D::Platform
)
target_compile_features(trace2d PRIVATE cxx_std_20)

trace2d_enable_warnings(trace2d)

if(BUILD_TESTING)
    add_test(NAME trace2d_cli_doctor COMMAND trace2d doctor --json)
    add_test(NAME trace2d_cli_headless_smoke COMMAND trace2d run --headless --json)
endif()
