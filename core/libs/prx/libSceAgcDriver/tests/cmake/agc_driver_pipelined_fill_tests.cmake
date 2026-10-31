if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_pipelined_fill)
endif()

if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_pipelined_fill)
endif()
