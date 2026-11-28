if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_indirect_draw_pending_producer)
    add_test(NAME agc_driver_indirect_draw_pending_producer_gpu_indirect_disabled COMMAND agc_driver_indirect_draw_pending_producer_tests --gpu-indirect-disabled)
    set_tests_properties(agc_driver_indirect_draw_pending_producer_gpu_indirect_disabled PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_GPU_INDIRECT_DRAW=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_indirect_draw_pending_producer_gpu_indirect_disabled PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
