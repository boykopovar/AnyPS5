if(BUILD_TESTING)
    set_property(TEST agc_driver_table_index_validation APPEND PROPERTY ENVIRONMENT "APS5_TABLE_INDEX=1")
    add_test(NAME agc_driver_table_index_depth_plane_verify COMMAND agc_driver_table_index_validation_tests depth)
    set_tests_properties(agc_driver_table_index_depth_plane_verify PROPERTIES ENVIRONMENT "APS5_VERIFY_DRAW_PIPELINE=1" SKIP_RETURN_CODE 77 TIMEOUT 60
        PASS_REGULAR_EXPRESSION "verify: [1-9][0-9]* reused resource proofs checked, 0 failed, 0 where the full proof stored to guest memory.*table index validation: depth passed"
        FAIL_REGULAR_EXPRESSION "AGC graphics:|[1-9][0-9]* where the full proof stored")
    if(WIN32)
        set_tests_properties(agc_driver_table_index_depth_plane_verify PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
