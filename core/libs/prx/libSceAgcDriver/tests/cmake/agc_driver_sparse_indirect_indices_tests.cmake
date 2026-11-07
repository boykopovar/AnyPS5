if(BUILD_TESTING)
    add_test(NAME agc_driver_sparse_indirect_indices_unreadable COMMAND agc_driver_sparse_indirect_indices_tests --unreadable)
    add_test(NAME agc_driver_sparse_indirect_indices_no_size COMMAND agc_driver_sparse_indirect_indices_tests --no-size)
    add_test(NAME agc_driver_sparse_indirect_indices_no_size_unreadable COMMAND agc_driver_sparse_indirect_indices_tests --no-size-unreadable)
    add_test(NAME agc_driver_sparse_indirect_indices_no_size_unreadable_record COMMAND agc_driver_sparse_indirect_indices_tests --no-size-unreadable-record)
    set_tests_properties(agc_driver_sparse_indirect_indices agc_driver_sparse_indirect_indices_unreadable agc_driver_sparse_indirect_indices_no_size agc_driver_sparse_indirect_indices_no_size_unreadable agc_driver_sparse_indirect_indices_no_size_unreadable_record PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sparse_indirect_indices_unreadable agc_driver_sparse_indirect_indices_no_size agc_driver_sparse_indirect_indices_no_size_unreadable agc_driver_sparse_indirect_indices_no_size_unreadable_record PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
