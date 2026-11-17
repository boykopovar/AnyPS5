if(BUILD_TESTING)
    set(imageTablePackedTests agc_driver_image_table_packed)
    foreach(mode IN ITEMS load_sgn load_mip load_mip_sgn by stores guards mixed isolated rejected mips empty unmeasured)
        set(testName agc_driver_image_table_packed_${mode})
        add_test(NAME ${testName} COMMAND agc_driver_image_table_packed_tests ${mode})
        list(APPEND imageTablePackedTests ${testName})
    endforeach()
    set_tests_properties(${imageTablePackedTests} PROPERTIES TIMEOUT 30
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-|was not found but was requested by env var VK_INSTANCE_LAYERS")
    if(WIN32)
        set_tests_properties(${imageTablePackedTests} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
