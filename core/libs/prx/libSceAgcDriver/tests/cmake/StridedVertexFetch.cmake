if(BUILD_TESTING AND TARGET agc_driver_strided_indirect_vertex_fetch_tests)
    foreach(agcStridedMode IN ITEMS direct-2 direct-3)
        string(REPLACE "-" "_" agcStridedName "${agcStridedMode}")
        set(agcStridedTest "agc_driver_strided_vertex_fetch_${agcStridedName}")
        add_test(NAME ${agcStridedTest} COMMAND agc_driver_strided_indirect_vertex_fetch_tests ${agcStridedMode})
        set_tests_properties(${agcStridedTest} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PIPELINE_JOURNAL=1"
                SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(${agcStridedTest} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
