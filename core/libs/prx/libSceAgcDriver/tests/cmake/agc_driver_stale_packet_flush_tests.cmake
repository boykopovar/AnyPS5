if(BUILD_TESTING)
    set_target_properties(agc_driver_stale_packet_flush_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(mode IN ITEMS stale-cap stale-deadline current-cap queued-overdue)
        string(REPLACE "-" "_" suffix ${mode})
        add_test(NAME agc_driver_stale_packet_flush_${suffix} COMMAND agc_driver_stale_packet_flush_tests ${mode})
        set_tests_properties(agc_driver_stale_packet_flush_${suffix} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_BATCH_CAP=64;APS5_LABEL_FLUSH_US=60000000;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32"
                SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_stale_packet_flush_${suffix} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_stale_packet_flush_stale_deadline PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_BATCH_CAP=64;APS5_LABEL_FLUSH_US=100000;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32")
endif()
