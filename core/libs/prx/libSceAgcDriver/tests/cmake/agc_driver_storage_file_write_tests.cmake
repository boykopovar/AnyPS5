target_link_libraries(agc_driver_storage_file_write_tests PRIVATE libkernel)

if(BUILD_TESTING)
    set_tests_properties(agc_driver_storage_file_write PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_SUSPEND_DRAIN=1")
endif()
