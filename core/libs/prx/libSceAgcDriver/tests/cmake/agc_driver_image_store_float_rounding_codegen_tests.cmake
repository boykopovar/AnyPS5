if(BUILD_TESTING AND (NOT ANYPS5_AGC_EXECUTION_TESTS OR "ImageStoreFloatRounding" IN_LIST ANYPS5_AGC_EXECUTION_TESTS))
    add_test(NAME agc_driver_image_store_float_rounding_codegen
            COMMAND agc_driver_image_store_float_rounding_tests --codegen-only)
    set_tests_properties(agc_driver_image_store_float_rounding_codegen PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_image_store_float_rounding_codegen PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
