string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef identity)
set(work "${CMAKE_CURRENT_BINARY_DIR}/symlinkat-refusal-${identity}")
file(MAKE_DIRECTORY "${work}")
execute_process(COMMAND "${TEST}" "${LIBRARIES}" "${MODE}" symlinkat_refused
    WORKING_DIRECTORY "${work}" TIMEOUT 15
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
file(GLOB created "${work}/anyps5-kernel-sys-paths-*/refused")
file(REMOVE_RECURSE "${work}")
if(result STREQUAL "0" OR result MATCHES "[Tt]imeout" OR created OR NOT "${output}${errors}" MATCHES "symlink: parent-directory targets not implemented")
    message(FATAL_ERROR "Expected the symlink parent-target refusal; result=${result}\n${output}${errors}")
endif()
