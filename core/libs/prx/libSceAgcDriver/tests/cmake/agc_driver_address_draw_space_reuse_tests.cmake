if(BUILD_TESTING)
    add_test(NAME agc_driver_address_draw_space_reuse_heap COMMAND agc_driver_address_draw_space_reuse_tests)
    set_tests_properties(agc_driver_address_draw_space_reuse_heap PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GPU_HEAP=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_address_draw_space_reuse_heap PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_address_draw_space_reuse_heap_transitions COMMAND agc_driver_address_draw_space_reuse_tests)
    set_tests_properties(agc_driver_address_draw_space_reuse_heap_transitions PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_GPU_HEAP=1;APS5_DRAW_TRANSITIONS=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_address_draw_space_reuse_heap_transitions PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
