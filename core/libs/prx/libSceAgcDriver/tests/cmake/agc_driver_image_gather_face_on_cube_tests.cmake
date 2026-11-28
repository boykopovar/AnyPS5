if(BUILD_TESTING)
    add_test(NAME agc_driver_image_gather_face_on_cube_without_non_seamless COMMAND agc_driver_image_gather_face_on_cube_tests)
    set_tests_properties(agc_driver_image_gather_face_on_cube_without_non_seamless PROPERTIES ENVIRONMENT "APS5_NO_NON_SEAMLESS_CUBE_MAP=1;ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_image_gather_face_on_cube_without_non_seamless PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
