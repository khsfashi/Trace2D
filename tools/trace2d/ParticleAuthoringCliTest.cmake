if(NOT DEFINED TRACE2D_CLI OR NOT DEFINED SOURCE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "TRACE2D_CLI, SOURCE, and WORK_DIR are required")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/effects")
configure_file(
    "${SOURCE}"
    "${WORK_DIR}/effects/authoring.trace2d.particle.toml"
    COPYONLY
)

set(resource "effects/authoring.trace2d.particle.toml")
execute_process(
    COMMAND "${TRACE2D_CLI}" author particle
        --project "${WORK_DIR}"
        --resource "${resource}"
        --max-particles 16
        --emission-count 1
        --lifetime-frames 1 4
        --json
    RESULT_VARIABLE first_result
    OUTPUT_VARIABLE first_stdout
    ERROR_VARIABLE first_stderr
)
if(NOT first_result STREQUAL "0")
    message(FATAL_ERROR "Particle authoring commit failed (${first_result}): ${first_stderr}${first_stdout}")
endif()

foreach(required IN ITEMS
    "\"status\":\"ok\""
    "\"committed\":true"
    "\"validation_passed\":true"
    "\"program_fingerprint\":"
    "effect.max_particles"
    "emission.count"
    "lifetime.frames")
    string(FIND "${first_stdout}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Particle authoring result is missing '${required}': ${first_stdout}")
    endif()
endforeach()

# Replay the same semantic request instead of inspecting or regex-editing the authored TOML.
# A validated no-op proves the canonical authority now contains the requested typed state.
execute_process(
    COMMAND "${TRACE2D_CLI}" author particle
        --project "${WORK_DIR}"
        --resource "${resource}"
        --max-particles 16
        --emission-count 1
        --lifetime-frames 1 4
        --json
    RESULT_VARIABLE second_result
    OUTPUT_VARIABLE second_stdout
    ERROR_VARIABLE second_stderr
)
if(NOT second_result STREQUAL "0")
    message(FATAL_ERROR "Particle authoring no-op validation failed (${second_result}): ${second_stderr}${second_stdout}")
endif()

foreach(required IN ITEMS
    "\"status\":\"ok\""
    "\"committed\":false"
    "\"validation_passed\":true"
    "\"program_fingerprint\":"
    "\"changed_fields\":[]")
    string(FIND "${second_stdout}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Particle no-op validation is missing '${required}': ${second_stdout}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
