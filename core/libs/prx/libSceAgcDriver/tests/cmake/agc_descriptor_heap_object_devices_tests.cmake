add_test_executable(agc_descriptor_heap_object_devices_tests EXCLUDE_FROM_ALL tests/DescriptorHeapObjectDevices.cpp)
target_include_directories(agc_descriptor_heap_object_devices_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_descriptor_heap_object_devices_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_descriptor_heap_object_devices_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_descriptor_heap_object_devices_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_descriptor_heap_object_devices_tests)
endif()
