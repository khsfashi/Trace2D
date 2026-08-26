if(NOT DEFINED TRACE2D_PACKAGE_CONFIG OR TRACE2D_PACKAGE_CONFIG STREQUAL "")
    message(FATAL_ERROR "TRACE2D_PACKAGE_CONFIG must point to the generated Trace2DConfig.cmake")
endif()

if(NOT EXISTS "${TRACE2D_PACKAGE_CONFIG}")
    message(FATAL_ERROR "Generated Trace2D package config does not exist: ${TRACE2D_PACKAGE_CONFIG}")
endif()

file(READ "${TRACE2D_PACKAGE_CONFIG}" _trace2d_package_config)

string(FIND
    "${_trace2d_package_config}"
    "set_and_check(\n    Trace2D_AGENT_INDEX"
    _trace2d_agent_index_resolution
)
string(FIND
    "${_trace2d_package_config}"
    "Trace2D Agent index:"
    _trace2d_agent_index_status
)
string(FIND
    "${_trace2d_package_config}"
    "find_dependency(box2d CONFIG REQUIRED)"
    _trace2d_first_dependency
)

if(_trace2d_agent_index_resolution EQUAL -1)
    message(FATAL_ERROR "Generated Trace2D package config does not resolve Trace2D_AGENT_INDEX")
endif()
if(_trace2d_agent_index_status EQUAL -1)
    message(FATAL_ERROR "Generated Trace2D package config does not expose the Agent index status")
endif()
if(_trace2d_first_dependency EQUAL -1)
    message(FATAL_ERROR "Generated Trace2D package config is missing the first box2d dependency lookup")
endif()

if(_trace2d_agent_index_status LESS _trace2d_agent_index_resolution)
    message(FATAL_ERROR "Trace2D Agent index status is emitted before the relocatable index path is resolved")
endif()
if(_trace2d_agent_index_status GREATER _trace2d_first_dependency)
    message(FATAL_ERROR "Trace2D Agent index status must be emitted before the first dependency lookup can fail")
endif()

string(REGEX MATCHALL "Trace2D Agent index:" _trace2d_agent_index_messages "${_trace2d_package_config}")
list(LENGTH _trace2d_agent_index_messages _trace2d_agent_index_message_count)
if(NOT _trace2d_agent_index_message_count EQUAL 1)
    message(FATAL_ERROR
        "Generated Trace2D package config must expose exactly one Agent index status message; found ${_trace2d_agent_index_message_count}"
    )
endif()
