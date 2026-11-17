if(BUILD_TESTING)
    set_tests_properties(agc_driver_storage_cache_budget agc_driver_storage_cache_entries PROPERTIES TIMEOUT 30)
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
    add_test(NAME agc_driver_storage_cache_entries_unlimited COMMAND agc_driver_storage_cache_entries_tests --unlimited)
    set_tests_properties(agc_driver_storage_cache_entries_unlimited PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_storage_cache_entries_profile COMMAND agc_driver_storage_cache_entries_tests --profile)
    set_tests_properties(agc_driver_storage_cache_entries_profile PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        PASS_REGULAR_EXPRESSION "write-backs by reason .count/MiB.: .*entry cap 2/.*evictions by reason .count/MiB flushed.: .*entry cap 2/"
        FAIL_REGULAR_EXPRESSION "entry limit test failed")
    if(WIN32)
        set_tests_properties(agc_driver_storage_cache_retained_batch agc_driver_storage_cache_entries_unlimited agc_driver_storage_cache_entries_profile PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    agc_add_env_test(agc_driver_storage_cache_budget_heap_slots agc_driver_storage_cache_budget_tests
            ARGS --heap-slots ENV ANYPS5_NO_SHADER_CACHE=1 APS5_NO_IMAGE_SLABS=1 TIMEOUT 30)
    agc_add_env_test(agc_driver_storage_cache_retained_batch_heap_slots agc_driver_storage_cache_budget_tests
            ARGS --retained-batch-heap-slots ENV ANYPS5_NO_SHADER_CACHE=1 TIMEOUT 30)
endif()
