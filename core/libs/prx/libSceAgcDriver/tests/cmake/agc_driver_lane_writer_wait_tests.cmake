if(BUILD_TESTING)
    set_tests_properties(agc_driver_lane_writer_wait PROPERTIES TIMEOUT 15
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_ASSERT_GPU_LOCK=1")
endif()
