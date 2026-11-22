if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(agc_driver_flexible_dma_buf_bind_failure_tests PRIVATE libkernel)
    set_tests_properties(agc_driver_flexible_dma_buf_bind_failure PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_WRITE_WATCH_IMPORTS=probe" SKIP_RETURN_CODE 77 TIMEOUT 40)
endif()
