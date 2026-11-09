if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_heap_ring_draw_key)
    agc_add_gpu_heap_test(agc_driver_heap_ring_draw_key)
    agc_add_env_test(agc_driver_heap_ring_draw_key_gpu_heap_fixed_buffers agc_driver_heap_ring_draw_key_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_DRAW_PIPELINE=1 APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1 APS5_NO_MOVED_BUFFER_TEMPLATES=1)
endif()
