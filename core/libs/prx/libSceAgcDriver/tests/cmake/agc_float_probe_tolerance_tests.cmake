add_test_executable(agc_float_probe_tolerance_tests EXCLUDE_FROM_ALL tests/FloatProbeTolerance.cpp Execution/src/FloatProbeTolerance.cpp)
target_include_directories(agc_float_probe_tolerance_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_float_probe_tolerance_tests PRIVATE libc)
set_target_properties(agc_float_probe_tolerance_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_float_probe_tolerance_tests)

if(BUILD_TESTING)
    set_target_properties(agc_float_probe_tolerance_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_float_probe_tolerance COMMAND agc_float_probe_tolerance_tests)
    if(WIN32)
        set_tests_properties(agc_float_probe_tolerance PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
