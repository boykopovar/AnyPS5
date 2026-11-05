add_test_executable(agc_constant_phi_tests EXCLUDE_FROM_ALL tests/ConstantPhi.cpp)
target_link_libraries(agc_constant_phi_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_constant_phi_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_constant_phi_tests)

if(BUILD_TESTING)
    set_target_properties(agc_constant_phi_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_constant_phi COMMAND agc_constant_phi_tests)
    if(WIN32)
        set_tests_properties(agc_constant_phi PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
