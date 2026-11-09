if(BUILD_TESTING)
    set_property(TEST agc_driver_compute_prebuild_share APPEND PROPERTY ENVIRONMENT "APS5_PREBUILD_THREADS=4")
endif()
