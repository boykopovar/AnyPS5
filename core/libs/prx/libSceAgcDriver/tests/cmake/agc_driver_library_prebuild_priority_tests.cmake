if(BUILD_TESTING)
    set_property(TEST agc_driver_library_prebuild_priority APPEND PROPERTY ENVIRONMENT "APS5_PREBUILD_THREADS=2")
endif()
