if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_draw_recipe_moved_buffer)
    agc_add_env_test(agc_driver_draw_recipe_moved_buffer_gpu_heap_overflow agc_driver_draw_recipe_moved_buffer_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_PUSHED_BUFFER_RECORDS=0)
    agc_add_env_test(agc_driver_draw_recipe_moved_buffer_gpu_heap_unverified agc_driver_draw_recipe_moved_buffer_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_DRAW_PIPELINE=1 APS5_PIPELINED_DRAWS=32)
endif()
