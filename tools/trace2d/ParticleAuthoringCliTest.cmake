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
if(NOT first_result EQUAL 0)
    message(FATAL_ERROR "Particle authoring commit failed (${first_result}): ${first_stderr}${first_stdout}")
endif()

foreach(required IN ITEMS
    "\"status\":\"ok\""
    "\"committed\":true"
    "\"validation_passed\":true"
    "effect.max_particles"
    "emission.count"
    "lifetime.frames")
    string(FIND "${first_stdout}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Particle authoring result is missing '${required}': ${first_stdout}")
    endif()
endforeach()

file(READ "${WORK_DIR}/${resource}" committed_text)
foreach(required IN ITEMS
    "max_particles = 16"
    "count = 1"
    "frames = [1, 4]")
    string(FIND "${committed_text}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Committed Particle resource is missing '${required}'")
    endif()
endforeach()

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
if(NOT second_result EQUAL 0)
    message(FATAL_ERROR "Particle authoring no-op validation failed (${second_result}): ${second_stderr}${second_stdout}")
endif()
string(FIND "${second_stdout}" "\"committed\":false" no_op_position)
if(no_op_position EQUAL -1)
    message(FATAL_ERROR "Equivalent Particle mutation should not rewrite the resource: ${second_stdout}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
