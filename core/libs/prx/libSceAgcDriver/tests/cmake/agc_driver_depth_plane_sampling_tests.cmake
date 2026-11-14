if(BUILD_TESTING)
    add_test(NAME agc_driver_depth_plane_sampling_evicting COMMAND agc_driver_depth_plane_sampling_tests --evicting)
    set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT "APS5_DEPTH_PLANE_COPY_MIB=1" SKIP_RETURN_CODE 77 TIMEOUT 120)
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
