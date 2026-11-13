add_test_executable(agc_tessellation_lowering_tests EXCLUDE_FROM_ALL tests/TessellationLowering.cpp)
target_link_libraries(agc_tessellation_lowering_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_tessellation_lowering_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_tessellation_lowering_tests)

if(BUILD_TESTING)
    set_target_properties(agc_tessellation_lowering_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_tessellation_lowering COMMAND agc_tessellation_lowering_tests)
    if(WIN32)
        set_tests_properties(agc_tessellation_lowering PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
