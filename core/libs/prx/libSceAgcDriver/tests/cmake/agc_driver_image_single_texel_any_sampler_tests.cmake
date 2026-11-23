if(BUILD_TESTING)
    add_test(NAME agc_driver_image_single_texel_lod_query_sampler
            COMMAND agc_driver_image_single_texel_any_sampler_tests --lod-query-only)
    set_tests_properties(agc_driver_image_single_texel_lod_query_sampler PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 60 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_image_single_texel_lod_query_sampler PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
