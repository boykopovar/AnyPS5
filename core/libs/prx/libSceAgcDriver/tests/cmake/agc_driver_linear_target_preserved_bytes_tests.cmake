add_test(NAME agc_driver_linear_target_preserved_bytes_seeded COMMAND agc_driver_linear_target_preserved_bytes_tests)
set_tests_properties(agc_driver_linear_target_preserved_bytes_seeded PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_LINEAR_SEED_LEFT=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if(WIN32)
    set_tests_properties(agc_driver_linear_target_preserved_bytes_seeded PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
endif()
