if(BUILD_TESTING AND TARGET agc_driver_heap_draw_sample_tests)
    agc_add_env_test(agc_driver_heap_draw_sample_heap agc_driver_heap_draw_sample_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1)
    agc_add_env_test(agc_driver_heap_draw_sample_full_heap agc_driver_heap_draw_sample_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_HEAP_SLOT_CAP=1)
    agc_add_env_test(agc_driver_heap_draw_sample_full_heap_pipelined agc_driver_heap_draw_sample_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_HEAP_SLOT_CAP=1 APS5_DRAW_PIPELINE=1 APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1)
    agc_add_env_test(agc_driver_heap_draw_sample_nogpl agc_driver_heap_draw_sample_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_NO_GPL=1)
    agc_add_env_test(agc_driver_heap_draw_sample_nogpl_heap agc_driver_heap_draw_sample_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_NO_GPL=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1)
endif()
