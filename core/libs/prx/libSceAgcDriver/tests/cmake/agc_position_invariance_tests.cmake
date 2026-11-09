add_test_executable(agc_position_invariance_tests EXCLUDE_FROM_ALL tests/PositionInvariance.cpp)
target_link_libraries(agc_position_invariance_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_position_invariance_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_position_invariance_tests)

if(BUILD_TESTING)
    set_target_properties(agc_position_invariance_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_position_invariance COMMAND agc_position_invariance_tests)
    set_tests_properties(agc_position_invariance PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_position_invariance PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
