add_test_executable(agc_split_table_call_tests EXCLUDE_FROM_ALL tests/SplitTableCall.cpp)
target_link_libraries(agc_split_table_call_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_split_table_call_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_split_table_call_tests)

if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_link_libraries(agc_split_table_call_tests PRIVATE SPIRV-Tools-static)
endif()

if(BUILD_TESTING)
    agc_add_spirv_mode_tests(agc_split_table_call_tests)
endif()
