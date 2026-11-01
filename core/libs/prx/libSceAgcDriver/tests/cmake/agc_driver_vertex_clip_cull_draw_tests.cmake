if(BUILD_TESTING)
    add_test(NAME agc_driver_vertex_clip_cull_draw_no_key COMMAND agc_driver_vertex_clip_cull_draw_tests no-key)
    set_tests_properties(agc_driver_vertex_clip_cull_draw_no_key PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_KEY=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_vertex_clip_cull_draw_no_key PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
