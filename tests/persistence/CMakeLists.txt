add_executable(trace2d_persistence_tests
    SaveDocumentTests.cpp
)

target_link_libraries(trace2d_persistence_tests
    PRIVATE
        Trace2D::Persistence
        GTest::gtest_main
)

target_compile_features(trace2d_persistence_tests PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_persistence_tests)

include(GoogleTest)
gtest_discover_tests(trace2d_persistence_tests)
