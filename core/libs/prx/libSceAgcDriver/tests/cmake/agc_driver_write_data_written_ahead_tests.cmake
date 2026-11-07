if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_write_data_written_ahead)
endif()

if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_write_data_written_ahead)
endif()
