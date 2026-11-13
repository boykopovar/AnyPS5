if(BUILD_TESTING)
    set_tests_properties(agc_driver_storage_flush_lane_wait PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_ASSERT_GPU_LOCK=1"
        TIMEOUT 60 FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-")
    set_property(TEST agc_driver_storage_flush_lane_wait APPEND PROPERTY ENVIRONMENT "VK_LAYER_MESSAGE_ID_FILTER=VUID-VkDeviceCreateInfo-pNext-pNext")
endif()
