target_link_libraries(agc_driver_storage_completion_reuse_tests PRIVATE libkernel)

if(BUILD_TESTING)
    set_tests_properties(agc_driver_storage_completion_reuse PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_storage_completion_reuse_writeback COMMAND agc_driver_storage_completion_reuse_tests medium compared writeback)
    set_tests_properties(agc_driver_storage_completion_reuse_writeback PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_storage_completion_reuse_writeback PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(reuseSize IN ITEMS small medium large)
        foreach(reuseMemory IN ITEMS compared watched)
            foreach(reuseImport IN ITEMS allowed disabled)
                if(reuseSize STREQUAL "medium" AND reuseMemory STREQUAL "compared" AND reuseImport STREQUAL "allowed")
                    continue()
                endif()
                set(reuseTest "agc_driver_storage_completion_reuse_${reuseSize}_${reuseMemory}")
                set(reuseEnvironment "ANYPS5_NO_SHADER_CACHE=1")
                if(reuseImport STREQUAL "disabled")
                    string(APPEND reuseTest "_no_import")
                    list(APPEND reuseEnvironment "APS5_NO_HOST_IMPORT=1")
                endif()
                add_test(NAME ${reuseTest} COMMAND agc_driver_storage_completion_reuse_tests ${reuseSize} ${reuseMemory})
                set_tests_properties(${reuseTest} PROPERTIES
                        ENVIRONMENT "${reuseEnvironment}" SKIP_RETURN_CODE 77
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
                if(WIN32)
                    set_tests_properties(${reuseTest} PROPERTIES ENVIRONMENT_MODIFICATION
                            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
                endif()
            endforeach()
        endforeach()
    endforeach()
    foreach(reuseRoute IN ITEMS label64 timestamp cache-only interrupt conditional no-interrupt compute)
        string(REPLACE "-" "_" reuseSuffix "${reuseRoute}")
        set(reuseTest "agc_driver_storage_completion_reuse_${reuseSuffix}")
        add_test(NAME ${reuseTest} COMMAND agc_driver_storage_completion_reuse_tests medium compared explicit ${reuseRoute})
        set_tests_properties(${reuseTest} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
        if(WIN32)
            set_tests_properties(${reuseTest} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    foreach(reuseExecution IN ITEMS deferred immediate dispatch_items)
        set(reuseTest "agc_driver_storage_completion_reuse_${reuseExecution}")
        set(reuseEnvironment "ANYPS5_NO_SHADER_CACHE=1")
        set(reuseRoute "no-interrupt")
        if(reuseExecution STREQUAL "dispatch_items")
            set(reuseRoute "label32")
            list(APPEND reuseEnvironment "APS5_DRAW_PIPELINE=1" "APS5_PIPELINED_DRAWS=8" "APS5_PIPELINE_DISPATCH=1")
        else()
            list(APPEND reuseEnvironment "APS5_PIPELINED_DRAWS=0")
            if(reuseExecution STREQUAL "immediate")
                list(APPEND reuseEnvironment "APS5_LABEL_LOCK_EACH=1")
            endif()
        endif()
        add_test(NAME ${reuseTest} COMMAND agc_driver_storage_completion_reuse_tests medium compared explicit ${reuseRoute})
        set_tests_properties(${reuseTest} PROPERTIES
                ENVIRONMENT "${reuseEnvironment}" SKIP_RETURN_CODE 77
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
        if(WIN32)
            set_tests_properties(${reuseTest} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_driver_storage_completion_reuse_consumer_ahead COMMAND agc_driver_storage_completion_reuse_tests medium compared explicit consumer)
    set_tests_properties(agc_driver_storage_completion_reuse_consumer_ahead PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GROUP_CAPTURE_ALL_QUEUES=1;APS5_PIPELINED_DRAWS=0" SKIP_RETURN_CODE 77
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_storage_completion_reuse_consumer_ahead PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
