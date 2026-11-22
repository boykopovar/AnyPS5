target_link_libraries(agc_driver_gcn_display_present_tests PRIVATE decoder_png)

if(BUILD_TESTING)
    add_test(NAME agc_driver_gcn_display_present_trigger COMMAND agc_driver_gcn_display_present_tests trigger)
    set_tests_properties(agc_driver_gcn_display_present_trigger PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_gcn_display_present_trigger PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
