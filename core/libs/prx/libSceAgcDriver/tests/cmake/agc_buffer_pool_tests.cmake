add_test_executable(agc_buffer_pool_tests EXCLUDE_FROM_ALL tests/BufferPool.cpp)
target_sources(agc_buffer_pool_tests PRIVATE Graphics/src/BufferPool.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_buffer_pool_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_buffer_pool_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_buffer_pool_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_buffer_pool_tests)

if(BUILD_TESTING)
    set_target_properties(agc_buffer_pool_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_buffer_pool COMMAND agc_buffer_pool_tests)
    add_test(NAME agc_memory_accounting COMMAND agc_buffer_pool_tests --memory-accounting)
    set_tests_properties(agc_memory_accounting PROPERTIES ENVIRONMENT "APS5_TRACE_MEMORY_BUDGET=1" TIMEOUT 30)
    add_test(NAME agc_memory_accounting_budget_cap COMMAND agc_buffer_pool_tests --memory-accounting-budget-cap)
    set_tests_properties(agc_memory_accounting_budget_cap PROPERTIES ENVIRONMENT "APS5_TRACE_MEMORY_BUDGET=1"
        PASS_REGULAR_EXPRESSION "cap=1073741824 selected=1073741824" TIMEOUT 30)
    add_test(NAME agc_memory_accounting_budget_floor COMMAND agc_buffer_pool_tests --memory-accounting-budget-floor)
    set_tests_properties(agc_memory_accounting_budget_floor PROPERTIES ENVIRONMENT "APS5_TRACE_MEMORY_BUDGET=1"
        PASS_REGULAR_EXPRESSION "cap=536870912 selected=536870912" TIMEOUT 30)
    add_test(NAME agc_buffer_pool_small_device_shared COMMAND agc_buffer_pool_tests --small-device-retention)
    set_tests_properties(agc_buffer_pool_small_device_shared PROPERTIES ENVIRONMENT
        "APS5_STAGING_POOL_MIB=256;APS5_BUFFER_POOL_SHARED=1")
    add_test(NAME agc_buffer_pool_small_device_no_staging COMMAND agc_buffer_pool_tests --small-device-retention)
    set_tests_properties(agc_buffer_pool_small_device_no_staging PROPERTIES ENVIRONMENT
        "APS5_STAGING_POOL_MIB=0")
    if(WIN32)
        set_tests_properties(agc_memory_accounting agc_memory_accounting_budget_cap agc_memory_accounting_budget_floor agc_buffer_pool agc_buffer_pool_small_device_shared agc_buffer_pool_small_device_no_staging PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
