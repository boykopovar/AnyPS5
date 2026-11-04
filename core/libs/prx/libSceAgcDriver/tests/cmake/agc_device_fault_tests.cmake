add_test_executable(agc_device_fault_tests EXCLUDE_FROM_ALL tests/DeviceFault.cpp)
target_include_directories(agc_device_fault_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_device_fault_tests PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(agc_device_fault_tests PRIVATE libc libkernel)
configure_windows_unwind(agc_device_fault_tests)

if(BUILD_TESTING)
    set_target_properties(agc_device_fault_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_device_fault COMMAND agc_device_fault_tests)
    if(WIN32)
        set_tests_properties(agc_device_fault PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
