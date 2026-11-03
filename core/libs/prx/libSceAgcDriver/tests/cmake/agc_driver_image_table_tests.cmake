if(BUILD_TESTING)
    set_tests_properties(agc_driver_image_table PROPERTIES TIMEOUT 30
            FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-|was not found but was requested by env var VK_INSTANCE_LAYERS")
endif()
