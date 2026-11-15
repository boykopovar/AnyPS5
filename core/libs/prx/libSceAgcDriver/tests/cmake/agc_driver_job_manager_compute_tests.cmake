target_link_libraries(agc_driver_job_manager_compute_tests PRIVATE libSceJobManager libSceAgc libkernel)

if(BUILD_TESTING)
    add_test(NAME agc_driver_job_manager_compute_capacity COMMAND agc_driver_job_manager_compute_tests barrier_capacity_overflow)
    set_tests_properties(agc_driver_job_manager_compute_capacity PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 20 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_job_manager_compute_capacity PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
