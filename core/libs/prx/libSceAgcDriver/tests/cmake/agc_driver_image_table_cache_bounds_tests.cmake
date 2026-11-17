if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_image_table_cache_bounds)
    agc_add_env_test(agc_driver_image_table_atomics agc_driver_image_table_cache_bounds_tests
            ARGS --atomic-table ENV ANYPS5_NO_SHADER_CACHE=1 TIMEOUT 60)
    agc_add_env_test(agc_driver_image_table_atomics_gpu_heap agc_driver_image_table_cache_bounds_tests
            ARGS --atomic-table ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 TIMEOUT 60)
    agc_add_env_test(agc_driver_image_table_store_gpu_heap agc_driver_image_table_cache_bounds_tests
            ARGS --store-table ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 TIMEOUT 60)
endif()
