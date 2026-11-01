if(BUILD_TESTING)
    set_tests_properties(agc_driver_clamped_global_fill PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_LABEL_READBACK_STORE=1")
endif()
