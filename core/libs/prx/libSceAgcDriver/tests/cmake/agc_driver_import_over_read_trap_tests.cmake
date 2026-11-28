if(BUILD_TESTING AND APPLE)
    set_tests_properties(agc_driver_import_over_read_trap PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
endif()
