if(BUILD_TESTING)
    foreach(extraMipDrawMode IN ITEMS recorded immediate readback)
        add_test(NAME agc_driver_extra_mip_${extraMipDrawMode} COMMAND agc_driver_extra_mip_color_target_tests --${extraMipDrawMode})
        set_tests_properties(agc_driver_extra_mip_${extraMipDrawMode} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_extra_mip_${extraMipDrawMode} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_property(TEST agc_driver_extra_mip_readback APPEND PROPERTY ENVIRONMENT
            "APS5_DUMP_STORAGE=4x2x2:20;APS5_DUMP_STORAGE_GATE=extra-mip-readback.ready")
endif()
