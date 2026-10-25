foreach(required IN ITEMS FIXTURE MODE OUTPUT_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()
if(NOT MODE STREQUAL "write" AND NOT MODE STREQUAL "flush")
    message(FATAL_ERROR "Expected write or flush mode, got ${MODE}")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef runId)
set(runDirectory "${OUTPUT_ROOT}/capture-trace-${MODE}-${runId}")
file(MAKE_DIRECTORY "${runDirectory}")
set(ENV{APS5_CAPTURE_TRACE} "1")
set(ENV{APS5_CAPTURE_TRACE_PATH} "${runDirectory}/trace.log")
execute_process(
        COMMAND "${FIXTURE}" "${MODE}"
        WORKING_DIRECTORY "${runDirectory}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 8
)
file(WRITE "${runDirectory}/failure.log" "result=${result}\n${output}\n${error}")
set(armed "capture-trace-failure armed mode=${MODE} queueFull=1 attempted=4 finished=4")
if(MODE STREQUAL "write")
    set(sinkFailure "capture-trace-failure sink-short-write")
else()
    set(sinkFailure "capture-trace-failure sink-flush-failed")
endif()
set(survived "capture-trace-failure survived mode=${MODE} queueFull=1 attempted=4 finished=4 inactive=1 empty=1 sinkFailed=1")
string(FIND "${error}" "${armed}" armedAt)
string(FIND "${error}" "${sinkFailure}" sinkAt)
string(FIND "${error}" "Capture trace failed:" failedAt)
string(FIND "${error}" "${survived}" survivedAt)
string(REGEX MATCHALL "Capture trace failed:" failureMessages "${error}")
list(LENGTH failureMessages failureCount)
if(NOT result STREQUAL "0"
        OR error MATCHES "setup-failed|watchdog|unexpected-terminate"
        OR NOT failureCount EQUAL 1
        OR armedAt LESS 0 OR NOT sinkAt GREATER armedAt OR NOT failedAt GREATER sinkAt OR NOT survivedAt GREATER failedAt)
    message(FATAL_ERROR "Capture trace ${MODE} failure did not stop only tracing and exit normally: ${result}\n${output}\n${error}")
endif()
message(STATUS "Capture trace ${MODE} failure stopped tracing, released all callers before the sink failed and exited normally")
