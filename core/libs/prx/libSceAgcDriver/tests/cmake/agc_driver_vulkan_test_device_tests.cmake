if(BUILD_TESTING)
    add_test(NAME agc_driver_vulkan_test_device_no_driver COMMAND agc_driver_vulkan_test_device_tests --no-driver)
    set_tests_properties(agc_driver_vulkan_test_device_no_driver PROPERTIES
            ENVIRONMENT "VK_DRIVER_FILES=${CMAKE_CURRENT_BINARY_DIR}/no-vulkan-driver.json;VK_ICD_FILENAMES=${CMAKE_CURRENT_BINARY_DIR}/no-vulkan-driver.json"
            TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_vulkan_test_device_no_driver PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
