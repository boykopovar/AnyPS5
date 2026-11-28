target_sources(agc_driver_sample_order_retention_tests PRIVATE tests/AllocationFailure.cpp)
if(BUILD_TESTING AND MINGW)
    target_compile_definitions(agc_driver_sample_order_retention_tests PRIVATE APS5_AGC_ALLOCATION_COUNT=1)
endif()
