if(BUILD_TESTING)
    add_test(NAME agc_driver_depth_plane_proof_under_other_pending COMMAND agc_driver_depth_plane_partial_store_tests --proof-under-other-pending)
    set_tests_properties(agc_driver_depth_plane_proof_under_other_pending PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_proof_under_other_pending PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_cpu_write_after_page_unwatch COMMAND agc_driver_depth_plane_partial_store_tests --cpu-write-after-page-unwatch)
    set_tests_properties(agc_driver_depth_plane_cpu_write_after_page_unwatch PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_cpu_write_after_page_unwatch PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_cpu_write_after_partial_unwatch COMMAND agc_driver_depth_plane_partial_store_tests --cpu-write-after-partial-unwatch)
    set_tests_properties(agc_driver_depth_plane_cpu_write_after_partial_unwatch PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_cpu_write_after_partial_unwatch PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_cpu_write_after_memo COMMAND agc_driver_depth_plane_partial_store_tests --cpu-write-after-memo)
    set_tests_properties(agc_driver_depth_plane_cpu_write_after_memo PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_cpu_write_after_memo PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_unmapped_plane COMMAND agc_driver_depth_plane_partial_store_tests --unmapped-plane)
    set_tests_properties(agc_driver_depth_plane_unmapped_plane PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_unmapped_plane PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_partially_unmapped_plane COMMAND agc_driver_depth_plane_partial_store_tests --partially-unmapped-plane)
    set_tests_properties(agc_driver_depth_plane_partially_unmapped_plane PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_partially_unmapped_plane PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_unmapped_plane_fill COMMAND agc_driver_depth_plane_partial_store_tests --unmapped-plane-fill)
    set_tests_properties(agc_driver_depth_plane_unmapped_plane_fill PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_unmapped_plane_fill PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_partial_store_unimported COMMAND agc_driver_depth_plane_partial_store_tests)
    add_test(NAME agc_driver_depth_plane_partial_store_image_copy COMMAND agc_driver_depth_plane_partial_store_tests --image-copy)
    add_test(NAME agc_driver_depth_plane_partial_store_buffer_copy COMMAND agc_driver_depth_plane_partial_store_tests --buffer-copy)
endif()
