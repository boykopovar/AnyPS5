target_link_libraries(agc_driver_same_queue_label_wait_tests PRIVATE libkernel)

if(BUILD_TESTING)
    add_test(NAME agc_driver_cpu_label_wake COMMAND agc_driver_same_queue_label_wait_tests --cpu-label-wake-only)
    set_tests_properties(agc_driver_cpu_label_wake PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_cpu_label_wake PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_same_queue_label_wait)
endif()
