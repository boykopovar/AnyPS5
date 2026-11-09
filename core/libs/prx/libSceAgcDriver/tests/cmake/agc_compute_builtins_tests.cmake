add_test_executable(agc_compute_builtins_tests EXCLUDE_FROM_ALL tests/ComputeBuiltins.cpp)
target_link_libraries(agc_compute_builtins_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_compute_builtins_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_compute_builtins_tests)

if(BUILD_TESTING)
    set_target_properties(agc_compute_builtins_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_compute_builtins COMMAND agc_compute_builtins_tests)
    set_tests_properties(agc_compute_builtins PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_compute_builtins PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
