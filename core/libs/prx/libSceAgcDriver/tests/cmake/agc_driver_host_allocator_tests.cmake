if(BUILD_TESTING AND WIN32)
    add_test_executable(agc_driver_host_allocator_tests tests/HostAllocator.cpp Execution/src/HostAllocator.cpp)
    target_include_directories(agc_driver_host_allocator_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_driver_host_allocator_tests PRIVATE libc)
    configure_windows_unwind(agc_driver_host_allocator_tests)
    add_test(NAME agc_driver_host_allocator COMMAND agc_driver_host_allocator_tests)
    set_tests_properties(agc_driver_host_allocator PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
endif()

if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_draw_allocations)
    set_property(TEST agc_driver_draw_allocations agc_driver_draw_allocations_draw_pipeline agc_driver_draw_allocations_plan_verify APPEND PROPERTY ENVIRONMENT_MODIFICATION "VK_INSTANCE_LAYERS=unset:")
endif()
