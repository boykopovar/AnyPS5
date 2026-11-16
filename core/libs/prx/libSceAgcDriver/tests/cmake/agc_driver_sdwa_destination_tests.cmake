if(BUILD_TESTING)
    foreach(wave IN ITEMS wave32 wave64)
        add_test(NAME agc_driver_sdwa_destination_f32_${wave} COMMAND agc_driver_sdwa_destination_tests f32 ${wave})
        set_tests_properties(agc_driver_sdwa_destination_f32_${wave} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_sdwa_destination_f32_${wave} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_driver_sdwa_destination_wave64 COMMAND agc_driver_sdwa_destination_tests wave64)
    set_tests_properties(agc_driver_sdwa_destination_wave64 PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sdwa_destination_wave64 PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test(NAME agc_driver_sdwa_cvt_f32_ubyte_wave64 COMMAND agc_driver_sdwa_cvt_f32_ubyte_tests wave64)
    set_tests_properties(agc_driver_sdwa_cvt_f32_ubyte_wave64 PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sdwa_cvt_f32_ubyte_wave64 PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
