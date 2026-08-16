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

# Scored B1 animation workspaces are preserved outside the repository. When the
# owner-local scored harness supplies one explicit candidate directory, compile
# that candidate through the exact same verifier driver and production Runtime /
# Assets authority used by the independently qualified known-good/known-bad
# targets above. The default build remains unchanged and creates no candidate
# target until this cache path is explicitly supplied.
set(TRACE2D_B1_ANIMATION_CANDIDATE_DIR "" CACHE PATH
    "Optional B1 scored animation candidate directory containing animation_case.cpp/.hpp")
if(TRACE2D_B1_ANIMATION_CANDIDATE_DIR)
    get_filename_component(
        B1_ANIMATION_CANDIDATE_DIR
        "${TRACE2D_B1_ANIMATION_CANDIDATE_DIR}"
        ABSOLUTE)
    if(NOT EXISTS "${B1_ANIMATION_CANDIDATE_DIR}/animation_case.cpp")
        message(FATAL_ERROR
            "TRACE2D_B1_ANIMATION_CANDIDATE_DIR is missing animation_case.cpp: ${B1_ANIMATION_CANDIDATE_DIR}")
    endif()
    if(NOT EXISTS "${B1_ANIMATION_CANDIDATE_DIR}/animation_case.hpp")
        message(FATAL_ERROR
            "TRACE2D_B1_ANIMATION_CANDIDATE_DIR is missing animation_case.hpp: ${B1_ANIMATION_CANDIDATE_DIR}")
    endif()

    add_executable(trace2d_b1_animation_candidate_verify
        B1AnimationFixtureVerifierMain.cpp
        "${B1_ANIMATION_CANDIDATE_DIR}/animation_case.cpp"
    )
    target_include_directories(
        trace2d_b1_animation_candidate_verify
        PRIVATE
            "${B1_ANIMATION_CANDIDATE_DIR}"
    )
    target_link_libraries(
        trace2d_b1_animation_candidate_verify
        PRIVATE
            Trace2D::Assets
            Trace2D::Runtime
    )
    target_compile_features(trace2d_b1_animation_candidate_verify PRIVATE cxx_std_20)
    trace2d_enable_warnings(trace2d_b1_animation_candidate_verify)
endif()

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

# B2 P1c: qualify the independent Trace2D lane verifier itself before any scored
# trial is allowed. Both candidates use the same public Application/Input/Scene/UI
# path; only the seeded cooldown defect differs. The known-bad executable is
# expected to return non-zero under the exact same verifier driver.
set(B2_TRACE2D_VERIFIER_ROOT
    "${CMAKE_SOURCE_DIR}/benchmarks/b2/qualification/trace2d_verifier")

function(trace2d_add_b2_trace2d_verifier target candidate_dir)
    add_executable(${target}
        B2Trace2DVerifierMain.cpp
        "${candidate_dir}/B2Candidate.cpp"
    )
    target_include_directories(${target} PRIVATE "${B2_TRACE2D_VERIFIER_ROOT}")
    target_link_libraries(${target}
        PRIVATE
            Trace2D::Application
            Trace2D::Agent
    )
    target_compile_features(${target} PRIVATE cxx_std_20)
    trace2d_enable_warnings(${target})
endfunction()

trace2d_add_b2_trace2d_verifier(
    trace2d_b2_trace2d_good_verify
    "${B2_TRACE2D_VERIFIER_ROOT}/known_good")
trace2d_add_b2_trace2d_verifier(
    trace2d_b2_trace2d_bad_cooldown_verify
    "${B2_TRACE2D_VERIFIER_ROOT}/known_bad_cooldown")

# The owner-local B2 scored harness preserves each candidate outside the
# repository and supplies that directory explicitly. Compile the candidate
# through the same qualified verifier driver; no target exists in normal builds.
set(TRACE2D_B2_CANDIDATE_DIR "" CACHE PATH
    "Optional B2 scored Trace2D candidate directory containing B2Candidate.cpp")
if(TRACE2D_B2_CANDIDATE_DIR)
    get_filename_component(
        B2_TRACE2D_CANDIDATE_DIR
        "${TRACE2D_B2_CANDIDATE_DIR}"
        ABSOLUTE)
    if(NOT EXISTS "${B2_TRACE2D_CANDIDATE_DIR}/B2Candidate.cpp")
        message(FATAL_ERROR
            "TRACE2D_B2_CANDIDATE_DIR is missing B2Candidate.cpp: ${B2_TRACE2D_CANDIDATE_DIR}")
    endif()
    trace2d_add_b2_trace2d_verifier(
        trace2d_b2_trace2d_candidate_verify
        "${B2_TRACE2D_CANDIDATE_DIR}")
endif()

add_test(
    NAME BenchmarkB2VerifierQualification.Trace2D.KnownGood
    COMMAND trace2d_b2_trace2d_good_verify
)
add_test(
    NAME BenchmarkB2VerifierQualification.Trace2D.KnownBadCooldown
    COMMAND trace2d_b2_trace2d_bad_cooldown_verify
)
set_tests_properties(
    BenchmarkB2VerifierQualification.Trace2D.KnownBadCooldown
    PROPERTIES WILL_FAIL TRUE
)
set_tests_properties(
    BenchmarkB2VerifierQualification.Trace2D.KnownGood
    BenchmarkB2VerifierQualification.Trace2D.KnownBadCooldown
    PROPERTIES LABELS "benchmark_b2_verifier_qualification"
)
