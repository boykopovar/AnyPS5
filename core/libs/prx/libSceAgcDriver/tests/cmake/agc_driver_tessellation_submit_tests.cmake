if(BUILD_TESTING)
    agc_add_gpu_heap_test(agc_driver_tessellation_submit)
    agc_add_env_test(agc_driver_tessellation_submit_geometry agc_driver_tessellation_submit_tests ARGS --geometry)
    agc_add_env_test(agc_driver_tessellation_submit_geometry_gpu_heap agc_driver_tessellation_submit_tests ARGS --geometry
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_DRAW_PIPELINE=1 APS5_PIPELINED_DRAWS=32 APS5_PIPELINE_DISPATCH=1)
endif()
