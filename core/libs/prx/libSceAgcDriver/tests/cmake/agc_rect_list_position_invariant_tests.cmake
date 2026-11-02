add_test_executable(agc_rect_list_position_invariant_tests EXCLUDE_FROM_ALL tests/RectListPositionInvariant.cpp)
target_link_libraries(agc_rect_list_position_invariant_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_rect_list_position_invariant_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_rect_list_position_invariant_tests)

if(BUILD_TESTING)
    set_target_properties(agc_rect_list_position_invariant_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_rect_list_position_invariant COMMAND agc_rect_list_position_invariant_tests)
    if(WIN32)
        set_tests_properties(agc_rect_list_position_invariant PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
