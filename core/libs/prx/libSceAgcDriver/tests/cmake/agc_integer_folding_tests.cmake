add_test_executable(agc_integer_folding_tests EXCLUDE_FROM_ALL tests/IntegerFolding.cpp)
target_link_libraries(agc_integer_folding_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_integer_folding_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_integer_folding_tests)

if(BUILD_TESTING)
    set_target_properties(agc_integer_folding_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_integer_folding COMMAND agc_integer_folding_tests)
    if(WIN32)
        set_tests_properties(agc_integer_folding PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
