if(NOT DEFINED TRACE2D_PROFILE_CLI)
    message(FATAL_ERROR "TRACE2D_PROFILE_CLI is required")
endif()

execute_process(
    COMMAND "${TRACE2D_PROFILE_CLI}"
        --headless
        --frames 8
        --warmup 2
        --seed 42
        --json
    RESULT_VARIABLE profile_result
    OUTPUT_VARIABLE profile_output
    ERROR_VARIABLE profile_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT profile_result EQUAL 0)
    message(FATAL_ERROR
        "trace2d-profile headless contract failed with ${profile_result}\n"
        "stdout: ${profile_output}\n"
        "stderr: ${profile_error}"
    )
endif()

set(required_fragments
    "\"schema\":\"trace2d.profile.report.v1\""
    "\"workload\":\"trace2d.representative-profile.v1\""
    "\"warmup_frame_count\":2"
    "\"requested_sample_frame_count\":8"
    "\"name\":\"resource.ready\",\"kind\":\"gauge\",\"unit\":\"{resource}\",\"availability\":\"available\",\"value\":1"
    "\"name\":\"particle.reference.capacity\",\"kind\":\"gauge\",\"unit\":\"{particle}\",\"availability\":\"available\",\"value\":64"
    "\"name\":\"render.draw.count\",\"kind\":\"counter\",\"unit\":\"{draw}\",\"availability\":\"not_measured\",\"value\":0"
    "\"cpu\":{\"availability\":\"available\""
    "\"retained_frame_capacity\":8"
    "\"committed_frame_count\":8"
    "\"gpu\":{\"availability\":\"not_supported\",\"backend\":\"headless\""
)

foreach(fragment IN LISTS required_fragments)
    string(FIND "${profile_output}" "${fragment}" fragment_position)
    if(fragment_position EQUAL -1)
        message(FATAL_ERROR
            "trace2d-profile output is missing deterministic contract fragment:\n"
            "${fragment}\n"
            "output: ${profile_output}"
        )
    endif()
endforeach()
