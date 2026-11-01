if(BUILD_TESTING)
    set_tests_properties(agc_driver_queued_shader_calls PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=8" TIMEOUT 30)
endif()
