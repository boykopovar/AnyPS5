if(BUILD_TESTING)
    add_test(NAME agc_driver_image_sint8_gather_constant_one COMMAND agc_driver_image_sint8_tests --gather-constant-one)
    add_test(NAME agc_driver_image_sint8_gather_constant_all_bits COMMAND agc_driver_image_sint8_tests --gather-constant-all-bits)
    set_tests_properties(agc_driver_image_sint8_gather_constant_one agc_driver_image_sint8_gather_constant_all_bits PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SPIRV_OPT=none" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(TEST agc_driver_image_sint8_gather_constant_one APPEND PROPERTY ENVIRONMENT "APS5_GATHER_CONST=1")
    set_property(TEST agc_driver_image_sint8_gather_constant_all_bits APPEND PROPERTY ENVIRONMENT "APS5_GATHER_CONST=0xffffffff")
    if(WIN32)
        set_tests_properties(agc_driver_image_sint8_gather_constant_one agc_driver_image_sint8_gather_constant_all_bits PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    agc_add_gpu_heap_test(agc_driver_image_sint8)
endif()
