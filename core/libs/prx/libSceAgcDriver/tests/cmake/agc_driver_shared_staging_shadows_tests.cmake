if(BUILD_TESTING)
    foreach(agcSharedShadowMode IN ITEMS off private changed)
        add_test(NAME agc_driver_shared_staging_shadows_${agcSharedShadowMode} COMMAND agc_driver_shared_staging_shadows_tests --${agcSharedShadowMode})
        set_tests_properties(agc_driver_shared_staging_shadows_${agcSharedShadowMode} PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_shared_staging_shadows_${agcSharedShadowMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_driver_shared_staging_shadows_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_STAGING_HANDOVER=1")
    set_tests_properties(agc_driver_shared_staging_shadows_private PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_SHARED_STAGING_SHADOWS=1")
    set_tests_properties(agc_driver_shared_staging_shadows_changed PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_CHANGED_COPY_BACK=1")
endif()
