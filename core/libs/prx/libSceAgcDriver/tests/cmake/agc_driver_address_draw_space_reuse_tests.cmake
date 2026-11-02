if(BUILD_TESTING)
    agc_add_env_test(agc_driver_address_draw_space_reuse_heap agc_driver_address_draw_space_reuse_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 TIMEOUT 60 NO_WORKING_DIRECTORY)
    agc_add_env_test(agc_driver_address_draw_space_reuse_heap_transitions agc_driver_address_draw_space_reuse_tests
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_DRAW_TRANSITIONS=1 TIMEOUT 60 NO_WORKING_DIRECTORY)
endif()
