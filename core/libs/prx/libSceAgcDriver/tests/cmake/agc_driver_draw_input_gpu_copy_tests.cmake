if(BUILD_TESTING)
    foreach(agcDrawInputCopyMode IN ITEMS off staged synchronous)
        add_test(NAME agc_driver_draw_input_gpu_copy_${agcDrawInputCopyMode} COMMAND agc_driver_draw_input_gpu_copy_tests --${agcDrawInputCopyMode})
        set_tests_properties(agc_driver_draw_input_gpu_copy_${agcDrawInputCopyMode} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_INPUTS_IN_PLACE=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_draw_input_gpu_copy_${agcDrawInputCopyMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_draw_input_gpu_copy_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_INPUTS_IN_PLACE=1;APS5_NO_DRAW_INPUT_GPU_COPY=1")
    set_tests_properties(agc_driver_draw_input_gpu_copy_staged PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_INPUTS_IN_PLACE=1;APS5_WRITTEN_SHADOW_MIN_KIB=0;APS5_NO_CHANGED_COPY_BACK=1")
    set_tests_properties(agc_driver_draw_input_gpu_copy_synchronous PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_INPUTS_IN_PLACE=1;APS5_SYNC_DRAWS=1")
endif()
