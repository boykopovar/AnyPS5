if(BUILD_TESTING AND ANYPS5_REQUIRE_CPU_LEAD_TEST)
    add_test(NAME agc_driver_sparse_dma_buf_cpu_lead COMMAND agc_driver_sparse_dma_buf_import_tests --require-cpu-lead)
    set_tests_properties(agc_driver_sparse_dma_buf_cpu_lead PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sparse_dma_buf_cpu_lead PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
