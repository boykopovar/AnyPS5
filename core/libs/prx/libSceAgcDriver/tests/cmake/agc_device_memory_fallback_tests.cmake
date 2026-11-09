add_test_executable(agc_device_memory_fallback_tests EXCLUDE_FROM_ALL tests/DeviceMemoryFallback.cpp)
target_include_directories(agc_device_memory_fallback_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_device_memory_fallback_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_device_memory_fallback_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_device_memory_fallback_tests)

if(BUILD_TESTING)
    set_target_properties(agc_device_memory_fallback_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_device_memory_fallback COMMAND agc_device_memory_fallback_tests)
    if(WIN32)
        set_tests_properties(agc_device_memory_fallback PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
