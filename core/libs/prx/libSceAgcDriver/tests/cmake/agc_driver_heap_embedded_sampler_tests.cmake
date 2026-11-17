if(BUILD_TESTING AND TARGET agc_driver_heap_embedded_sampler_tests)
    agc_add_env_test(agc_driver_heap_embedded_sampler_heap agc_driver_heap_embedded_sampler_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1)
    agc_add_env_test(agc_driver_heap_embedded_sampler_heap_capped agc_driver_heap_embedded_sampler_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_EMBEDDED_SAMPLER_CAP=0)
endif()
