if(BUILD_TESTING)
    foreach(runtimeFetchIndex IN ITEMS 65535 65536 65537)
        foreach(runtimeFetchPath IN ITEMS indexed compact-restart)
            set(runtimeFetchArguments runtime-fetch ${runtimeFetchIndex})
            if(runtimeFetchPath STREQUAL "compact-restart")
                list(APPEND runtimeFetchArguments compact-restart)
            endif()
            set(runtimeFetchTest agc_driver_runtime_fetch_${runtimeFetchPath}_${runtimeFetchIndex})
            add_test(NAME ${runtimeFetchTest} COMMAND agc_driver_draw_offset_indices_tests ${runtimeFetchArguments})
            set_tests_properties(${runtimeFetchTest} PROPERTIES
                    ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 45 SKIP_RETURN_CODE 77
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
            if(WIN32)
                set_tests_properties(${runtimeFetchTest} PROPERTIES ENVIRONMENT_MODIFICATION
                        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
endif()
