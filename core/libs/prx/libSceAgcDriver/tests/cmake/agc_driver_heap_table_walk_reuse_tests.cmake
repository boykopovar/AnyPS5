if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_heap_table_walk_reuse)
    if(TARGET agc_driver_heap_table_walk_reuse_tests)
        agc_add_env_test(agc_driver_heap_table_walk_reuse_heap agc_driver_heap_table_walk_reuse_tests
                ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1)
    endif()
endif()
