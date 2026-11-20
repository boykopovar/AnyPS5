if(BUILD_TESTING)
    add_test(NAME agc_driver_depth_plane_sampling_evicting COMMAND agc_driver_depth_plane_sampling_tests --evicting)
    set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT "APS5_DEPTH_PLANE_COPY_MIB=1" SKIP_RETURN_CODE 77 TIMEOUT 120)
    add_test(NAME agc_driver_depth_plane_sampling_guest_rewrite COMMAND agc_driver_depth_plane_sampling_tests --guest-rewrite)
    add_test(NAME agc_driver_depth_plane_sampling_layout_reuse COMMAND agc_driver_depth_plane_sampling_tests --layout-reuse)
    set_tests_properties(agc_driver_depth_plane_sampling_guest_rewrite agc_driver_depth_plane_sampling_layout_reuse
        PROPERTIES ENVIRONMENT "APS5_DEPTH_PLANE_COPY_MIB=0;APS5_NO_PIPELINE_JOURNAL=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_depth_plane_sampling_guest_rewrite APPEND PROPERTY ENVIRONMENT "APS5_NO_HOST_IMPORT=1")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_sampling_evicting agc_driver_depth_plane_sampling_guest_rewrite agc_driver_depth_plane_sampling_layout_reuse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
