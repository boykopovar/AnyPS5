if(BUILD_TESTING)
    set_tests_properties(agc_driver_scalar_atomic_translation PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
endif()
