if(TARGET agc_driver_buffer_format_oob2_stride_zero_tests)
    target_include_directories(agc_driver_buffer_format_oob2_stride_zero_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
    set_tests_properties(agc_driver_buffer_format_oob2_stride_zero PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_buffer_format_oob2_runtime_offsets COMMAND agc_driver_buffer_format_oob2_stride_zero_tests --runtime-offsets)
    set_tests_properties(agc_driver_buffer_format_oob2_runtime_offsets PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_buffer_format_oob2_runtime_offsets PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
