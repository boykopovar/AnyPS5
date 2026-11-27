if(BUILD_TESTING)
    add_test_executable(descriptor_layout_limits_tests ${LIBS_INCLUDE_DIR}/tests/DescriptorLayoutLimits.cpp Graphics/src/DescriptorValidation.cpp)
    target_include_directories(descriptor_layout_limits_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/include
        ${LIBS_INCLUDE_DIR}
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(descriptor_layout_limits_tests PRIVATE libc)
    configure_windows_unwind(descriptor_layout_limits_tests)
    add_test(NAME descriptor_layout_limits COMMAND descriptor_layout_limits_tests)
    set_tests_properties(descriptor_layout_limits PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(descriptor_layout_limits PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
