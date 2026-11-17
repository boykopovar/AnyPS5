if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_same_queue_label_wait)
endif()

if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_same_queue_label_wait)
endif()
