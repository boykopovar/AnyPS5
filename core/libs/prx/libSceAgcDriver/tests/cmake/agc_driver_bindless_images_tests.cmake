if(BUILD_TESTING)
    set_property(TEST agc_driver_bindless_images APPEND PROPERTY ENVIRONMENT_MODIFICATION "APS5_BINDLESS_IMAGES=unset:")
    add_test(NAME agc_driver_bindless_images_heap COMMAND agc_driver_bindless_images_tests --heap)
    set_tests_properties(agc_driver_bindless_images_heap PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PIPELINE_JOURNAL=1;APS5_BINDLESS_IMAGES=1"
            SKIP_RETURN_CODE 77 TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_bindless_images_heap PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
