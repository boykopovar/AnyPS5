if(BUILD_TESTING)
    set_tests_properties(agc_driver_lazy_pipeline_abi PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_LAZY_SHADER_PREPARE=1" TIMEOUT 30)
endif()
