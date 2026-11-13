target_link_libraries(agc_driver_memset_exclusive_tests PRIVATE libSceAgc)
if(BUILD_TESTING)
    set_tests_properties(agc_driver_memset_exclusive PROPERTIES TIMEOUT 30)
endif()
