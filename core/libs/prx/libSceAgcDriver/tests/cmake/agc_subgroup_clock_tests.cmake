add_test_executable(agc_subgroup_clock_tests EXCLUDE_FROM_ALL
        tests/SubgroupClock.cpp
        Execution/src/SubgroupClock.cpp
)
target_include_directories(agc_subgroup_clock_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_subgroup_clock_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_subgroup_clock_tests)

if(BUILD_TESTING)
    set_target_properties(agc_subgroup_clock_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_subgroup_clock COMMAND agc_subgroup_clock_tests)
endif()
