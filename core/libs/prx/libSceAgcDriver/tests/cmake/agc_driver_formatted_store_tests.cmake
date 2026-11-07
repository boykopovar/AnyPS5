if(BUILD_TESTING)
    add_test(NAME agc_driver_formatted_store_tight_tail COMMAND agc_driver_formatted_store_tests --tight-tail)
    add_test(NAME agc_driver_formatted_store_tight_tail_no_exact_robust_loads COMMAND agc_driver_formatted_store_tests --tight-tail)
    set_tests_properties(agc_driver_formatted_store_tight_tail agc_driver_formatted_store_tight_tail_no_exact_robust_loads PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(TEST agc_driver_formatted_store_tight_tail_no_exact_robust_loads APPEND PROPERTY ENVIRONMENT "APS5_NO_EXACT_ROBUST_LOADS=1")
    if(WIN32)
        set_tests_properties(agc_driver_formatted_store_tight_tail agc_driver_formatted_store_tight_tail_no_exact_robust_loads PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
