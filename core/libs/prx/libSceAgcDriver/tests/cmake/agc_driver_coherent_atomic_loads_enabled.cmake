if(BUILD_TESTING AND TARGET agc_driver_coherent_atomic_loads_tests)
    add_test(NAME agc_driver_coherent_atomic_loads_enabled COMMAND agc_driver_coherent_atomic_loads_tests)
    set_tests_properties(agc_driver_coherent_atomic_loads_enabled PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_COHERENT_ATOMIC_LOADS=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_coherent_atomic_loads_enabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
