add_test_executable(agc_decoder_encoding_coverage_tests EXCLUDE_FROM_ALL tests/DecoderEncodingCoverage.cpp)
target_link_libraries(agc_decoder_encoding_coverage_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_decoder_encoding_coverage_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_decoder_encoding_coverage_tests)

if(BUILD_TESTING)
    set_target_properties(agc_decoder_encoding_coverage_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_decoder_encoding_coverage COMMAND agc_decoder_encoding_coverage_tests)
    if(WIN32)
        set_tests_properties(agc_decoder_encoding_coverage PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
