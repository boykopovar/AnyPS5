add_test_executable(agc_redundant_value_elimination_tests EXCLUDE_FROM_ALL tests/RedundantValueElimination.cpp)
target_link_libraries(agc_redundant_value_elimination_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_redundant_value_elimination_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_redundant_value_elimination_tests)

if(BUILD_TESTING)
    set_target_properties(agc_redundant_value_elimination_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_redundant_value_elimination COMMAND agc_redundant_value_elimination_tests)
    if(WIN32)
        set_tests_properties(agc_redundant_value_elimination PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
