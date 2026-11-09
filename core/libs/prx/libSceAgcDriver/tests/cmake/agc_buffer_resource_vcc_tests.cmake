add_test_executable(agc_buffer_resource_vcc_tests EXCLUDE_FROM_ALL tests/BufferResourceVcc.cpp)
target_link_libraries(agc_buffer_resource_vcc_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_buffer_resource_vcc_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_buffer_resource_vcc_tests)

if(BUILD_TESTING)
    set_target_properties(agc_buffer_resource_vcc_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_buffer_resource_vcc COMMAND agc_buffer_resource_vcc_tests)
    set_tests_properties(agc_buffer_resource_vcc PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_buffer_resource_vcc PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
