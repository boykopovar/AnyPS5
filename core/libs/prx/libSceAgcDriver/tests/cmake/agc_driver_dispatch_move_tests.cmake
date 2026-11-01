if(BUILD_TESTING AND TARGET agc_driver_dispatch_move_tests)
    agc_add_env_test(agc_driver_dispatch_move_gpu_heap agc_driver_dispatch_move_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1)
endif()
