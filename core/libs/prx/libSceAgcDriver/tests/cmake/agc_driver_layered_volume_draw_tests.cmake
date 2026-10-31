if(BUILD_TESTING)
    add_test(NAME agc_driver_layered_volume_vertex COMMAND agc_driver_layered_volume_draw_tests --vertex-only)
    add_test(NAME agc_driver_layered_volume_vertex_no_gpl COMMAND agc_driver_layered_volume_draw_tests --vertex-only)
    set_tests_properties(agc_driver_layered_volume_vertex agc_driver_layered_volume_vertex_no_gpl PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_property(TEST agc_driver_layered_volume_vertex_no_gpl APPEND PROPERTY ENVIRONMENT "APS5_NO_GPL=1")
    if(WIN32)
        set_tests_properties(agc_driver_layered_volume_vertex agc_driver_layered_volume_vertex_no_gpl PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_volume_direct_readback COMMAND agc_driver_layered_volume_draw_tests)
    set_tests_properties(agc_driver_volume_direct_readback PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_TEST_DIRECT_READBACK=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_volume_direct_readback PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
