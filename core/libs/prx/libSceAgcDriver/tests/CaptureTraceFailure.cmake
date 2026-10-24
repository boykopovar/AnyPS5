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
execute_process(
        COMMAND "${FIXTURE}" control-abort
        WORKING_DIRECTORY "${runDirectory}"
        RESULT_VARIABLE abortResult OUTPUT_VARIABLE abortOutput ERROR_VARIABLE abortError TIMEOUT 8
)
file(WRITE "${runDirectory}/control.log" "result=${abortResult}\n${abortOutput}\n${abortError}")
if(abortResult STREQUAL "0" OR abortResult STREQUAL "87" OR abortResult STREQUAL "99"
        OR NOT abortError MATCHES "capture-trace-failure control-abort"
        OR abortError MATCHES "setup-failed|watchdog")
    message(FATAL_ERROR "Abort control failed: ${abortResult}\n${abortOutput}\n${abortError}")
endif()
if(WIN32)
    if(NOT abortResult STREQUAL "3")
        message(FATAL_ERROR "The Windows abort control returned an unexpected result: ${abortResult}")
    endif()
elseif(NOT abortResult MATCHES "[Aa]bort")
    message(FATAL_ERROR "The abort control did not report an abort signal: ${abortResult}")
endif()
set(ENV{APS5_CAPTURE_TRACE} "1")
set(ENV{APS5_CAPTURE_TRACE_PATH} "${runDirectory}/trace.log")
execute_process(
        COMMAND "${FIXTURE}" "${MODE}"
        WORKING_DIRECTORY "${runDirectory}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 8
)
file(WRITE "${runDirectory}/failure.log" "result=${result}\n${output}\n${error}")
set(armed "capture-trace-failure armed mode=${MODE} queueFull=1 attempted=4 finished=0")
if(MODE STREQUAL "write")
    set(sinkFailure "capture-trace-failure sink-short-write")
else()
    set(sinkFailure "capture-trace-failure sink-flush-failed")
endif()
set(terminate "capture-trace-failure terminate armed=1 queueFull=1 attempted=4 finished=0 sinkFailed=1")
string(FIND "${error}" "${armed}" armedAt)
string(FIND "${error}" "${sinkFailure}" sinkAt)
string(FIND "${error}" "Capture trace failed:" caughtAt)
string(FIND "${error}" "${terminate}" terminateAt)
if(NOT result STREQUAL abortResult OR result STREQUAL "87" OR result STREQUAL "99"
        OR error MATCHES "setup-failed|watchdog|handler-returned|handler-threw"
        OR armedAt LESS 0 OR NOT sinkAt GREATER armedAt OR NOT caughtAt GREATER sinkAt OR NOT terminateAt GREATER caughtAt)
    message(FATAL_ERROR "Capture trace ${MODE} failure did not reach its saved terminate handler and abort: ${result}\n${output}\n${error}")
endif()
message(STATUS "Capture trace ${MODE} failure reached its saved terminate handler and aborted with four waiting producers")
