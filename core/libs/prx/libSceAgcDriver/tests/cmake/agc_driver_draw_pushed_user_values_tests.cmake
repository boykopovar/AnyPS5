if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_draw_pushed_user_values)
    agc_add_env_test(agc_driver_draw_pushed_user_values_gpu_heap_unverified agc_driver_draw_pushed_user_values_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1)
endif()
