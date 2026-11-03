if(BUILD_TESTING)
    add_test_executable(agc_unit_flags_allocation_failure_tests
        tests/UnitFlagsAllocationFailure.cpp tests/AllocationFailure.cpp)
    target_include_directories(agc_unit_flags_allocation_failure_tests PRIVATE
        $<TARGET_PROPERTY:libSceAgcDriver,INCLUDE_DIRECTORIES>)
    if(MINGW)
        target_compile_options(agc_unit_flags_allocation_failure_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    configure_windows_unwind(agc_unit_flags_allocation_failure_tests)
    add_test(NAME agc_unit_flags_allocation_failure_empty COMMAND agc_unit_flags_allocation_failure_tests empty)
    add_test(NAME agc_unit_flags_allocation_failure_populated COMMAND agc_unit_flags_allocation_failure_tests populated)
    set_tests_properties(agc_unit_flags_allocation_failure_empty agc_unit_flags_allocation_failure_populated PROPERTIES TIMEOUT 20)
endif()
