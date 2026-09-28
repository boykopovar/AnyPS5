# Runs TEST_EXECUTABLE with TEST_ARGUMENT and passes only if it terminates unsuccessfully after
# printing EXPECTED_OUTPUT (a regular expression). ctest counts an aborted test as failed even when
# PASS_REGULAR_EXPRESSION matches, and a GPU worker failure terminates the process.
execute_process(
    COMMAND "${TEST_EXECUTABLE}" "${TEST_ARGUMENT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
    TIMEOUT 60
)
message("${output}")
if(result EQUAL 0)
    message(FATAL_ERROR "${TEST_EXECUTABLE} ${TEST_ARGUMENT} exited successfully instead of terminating")
endif()
if(NOT output MATCHES "${EXPECTED_OUTPUT}")
    message(FATAL_ERROR "${TEST_EXECUTABLE} ${TEST_ARGUMENT} terminated (${result}) without printing: ${EXPECTED_OUTPUT}")
endif()
