if(BUILD_TESTING)
    agc_add_env_test(agc_driver_draw_relocate_user_values_gpu_heap agc_driver_draw_relocate_user_values_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_VERIFY_DRAW_ENTRIES=1 APS5_DRAW_PIPELINE=1 APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1)
endif()
