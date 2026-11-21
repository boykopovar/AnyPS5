if(BUILD_TESTING)
    add_test(NAME agc_driver_image_sample_lod_clamp_shader_lane COMMAND agc_driver_image_sample_lod_clamp_tests --shader-lane)
    set_tests_properties(agc_driver_image_sample_lod_clamp_shader_lane PROPERTIES ENVIRONMENT "APS5_NO_IMAGE_VIEW_MIN_LOD=1;APS5_NO_GPL=1;ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_image_sample_lod_clamp_shader_lane PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
