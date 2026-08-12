add_executable(trace2d_b1_fixture_verify
    B1FixtureVerifierMain.cpp
)
target_link_libraries(trace2d_b1_fixture_verify
    PRIVATE
        Trace2D::Assets
        Trace2D::Particles
)
target_compile_features(trace2d_b1_fixture_verify PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_b1_fixture_verify)

set(B1_ANIMATION_GOOD_DIR
    "${CMAKE_SOURCE_DIR}/benchmarks/b1/tasks/b1-animation-exact-event/trace2d/known_good")
set(B1_ANIMATION_BAD_DIR
    "${CMAKE_SOURCE_DIR}/benchmarks/b1/tasks/b1-animation-exact-event/trace2d/known_bad_seeded_defect")

add_executable(trace2d_b1_animation_good_verify
    B1AnimationFixtureVerifierMain.cpp
    "${B1_ANIMATION_GOOD_DIR}/animation_case.cpp"
)
target_include_directories(trace2d_b1_animation_good_verify PRIVATE "${B1_ANIMATION_GOOD_DIR}")
target_link_libraries(trace2d_b1_animation_good_verify PRIVATE Trace2D::Assets Trace2D::Runtime)
target_compile_features(trace2d_b1_animation_good_verify PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_b1_animation_good_verify)

add_executable(trace2d_b1_animation_bad_verify
    B1AnimationFixtureVerifierMain.cpp
    "${B1_ANIMATION_BAD_DIR}/animation_case.cpp"
)
target_include_directories(trace2d_b1_animation_bad_verify PRIVATE "${B1_ANIMATION_BAD_DIR}")
target_link_libraries(trace2d_b1_animation_bad_verify PRIVATE Trace2D::Assets Trace2D::Runtime)
target_compile_features(trace2d_b1_animation_bad_verify PRIVATE cxx_std_20)
trace2d_enable_warnings(trace2d_b1_animation_bad_verify)

set(B1_SPRITE_ROOT "${CMAKE_SOURCE_DIR}/benchmarks/b1/tasks/b1-sprite-normalize-repair/trace2d")
set(B1_PARTICLE_ROOT "${CMAKE_SOURCE_DIR}/benchmarks/b1/tasks/b1-particle-budget-repair/trace2d")

add_test(
    NAME BenchmarkB1FixtureQualification.Trace2D.Sprite.KnownGood
    COMMAND trace2d_b1_fixture_verify sprite "${B1_SPRITE_ROOT}/known_good/hero.sprite.toml"
)
add_test(
    NAME BenchmarkB1FixtureQualification.Trace2D.Sprite.KnownBad
    COMMAND trace2d_b1_fixture_verify sprite "${B1_SPRITE_ROOT}/known_bad_seeded_defect/hero.sprite.toml"
)
set_tests_properties(
    BenchmarkB1FixtureQualification.Trace2D.Sprite.KnownBad
    PROPERTIES WILL_FAIL TRUE
)

add_test(
    NAME BenchmarkB1FixtureQualification.Trace2D.Animation.KnownGood
    COMMAND trace2d_b1_animation_good_verify
)
add_test(
    NAME BenchmarkB1FixtureQualification.Trace2D.Animation.KnownBad
    COMMAND trace2d_b1_animation_bad_verify
)
set_tests_properties(
    BenchmarkB1FixtureQualification.Trace2D.Animation.KnownBad
    PROPERTIES WILL_FAIL TRUE
)

add_test(
    NAME BenchmarkB1FixtureQualification.Trace2D.Particle.KnownGood
    COMMAND trace2d_b1_fixture_verify particle "${B1_PARTICLE_ROOT}/known_good/hit_spark.trace2d.particle.toml"
)
add_test(
    NAME BenchmarkB1FixtureQualification.Trace2D.Particle.KnownBad
    COMMAND trace2d_b1_fixture_verify particle "${B1_PARTICLE_ROOT}/known_bad_seeded_defect/hit_spark.trace2d.particle.toml"
)
set_tests_properties(
    BenchmarkB1FixtureQualification.Trace2D.Particle.KnownBad
    PROPERTIES WILL_FAIL TRUE
)

set_tests_properties(
    BenchmarkB1FixtureQualification.Trace2D.Sprite.KnownGood
    BenchmarkB1FixtureQualification.Trace2D.Sprite.KnownBad
    BenchmarkB1FixtureQualification.Trace2D.Animation.KnownGood
    BenchmarkB1FixtureQualification.Trace2D.Animation.KnownBad
    BenchmarkB1FixtureQualification.Trace2D.Particle.KnownGood
    BenchmarkB1FixtureQualification.Trace2D.Particle.KnownBad
    PROPERTIES LABELS "benchmark_b1_fixture_qualification"
)
