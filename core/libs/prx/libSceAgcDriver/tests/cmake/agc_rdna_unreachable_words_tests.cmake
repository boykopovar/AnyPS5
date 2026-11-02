add_test_executable(agc_rdna_unreachable_words_tests EXCLUDE_FROM_ALL tests/RdnaUnreachableWords.cpp)
target_link_libraries(agc_rdna_unreachable_words_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_rdna_unreachable_words_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_rdna_unreachable_words_tests)

if(BUILD_TESTING)
    set_target_properties(agc_rdna_unreachable_words_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_rdna_unreachable_words COMMAND agc_rdna_unreachable_words_tests)
    if(WIN32)
        set_tests_properties(agc_rdna_unreachable_words PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
