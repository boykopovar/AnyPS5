foreach(agcLinearCase IN ITEMS mask geometry reader eviction alias same)
    add_test(NAME agc_driver_linear_target_publication_${agcLinearCase} COMMAND agc_driver_linear_target_publication_tests ${agcLinearCase})
    set_tests_properties(agc_driver_linear_target_publication_${agcLinearCase} PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_linear_target_publication_${agcLinearCase} PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endforeach()
