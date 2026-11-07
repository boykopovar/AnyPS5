if(BUILD_TESTING)
    add_test_executable(agc_oracle_code_object_tests tests/OracleCodeObject.cpp tools/OracleCodeObject.cpp)
    target_include_directories(agc_oracle_code_object_tests PRIVATE
            $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
    )
    target_link_libraries(agc_oracle_code_object_tests PRIVATE libc)
    set_target_properties(agc_oracle_code_object_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_oracle_code_object_tests)
    add_test(NAME agc_oracle_code_object COMMAND agc_oracle_code_object_tests)
    set_tests_properties(agc_oracle_code_object PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_oracle_code_object PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
