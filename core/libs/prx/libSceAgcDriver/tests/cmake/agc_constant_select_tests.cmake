add_test_executable(agc_constant_select_tests EXCLUDE_FROM_ALL tests/ConstantSelect.cpp)
target_link_libraries(agc_constant_select_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_constant_select_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_constant_select_tests)

if(BUILD_TESTING)
    set_target_properties(agc_constant_select_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_constant_select COMMAND agc_constant_select_tests)
    if(WIN32)
        set_tests_properties(agc_constant_select PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
