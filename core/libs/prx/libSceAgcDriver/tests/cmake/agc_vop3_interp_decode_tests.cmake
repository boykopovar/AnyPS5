add_test_executable(agc_vop3_interp_decode_tests EXCLUDE_FROM_ALL tests/Vop3InterpDecode.cpp)
target_link_libraries(agc_vop3_interp_decode_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vop3_interp_decode_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vop3_interp_decode_tests)

if(BUILD_TESTING)
    set_target_properties(agc_vop3_interp_decode_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vop3_interp_decode COMMAND agc_vop3_interp_decode_tests)
    if(WIN32)
        set_tests_properties(agc_vop3_interp_decode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
