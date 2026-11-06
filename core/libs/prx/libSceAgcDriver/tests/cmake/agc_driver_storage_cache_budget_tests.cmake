if(BUILD_TESTING)
    set_tests_properties(agc_driver_storage_cache_budget PROPERTIES TIMEOUT 30)
    set_property(TEST agc_driver_storage_cache_budget APPEND PROPERTY ENVIRONMENT "APS5_NO_IMAGE_SLABS=1")
    add_test(NAME agc_driver_storage_cache_budget_slabs COMMAND agc_driver_storage_cache_budget_tests --budget-only)
    set_tests_properties(agc_driver_storage_cache_budget_slabs PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_storage_cache_budget_slabs PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_storage_cache_retained_batch COMMAND agc_driver_storage_cache_budget_tests --retained-batch)
    set_tests_properties(agc_driver_storage_cache_retained_batch PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_storage_cache_retained_batch PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
