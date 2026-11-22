add_test_executable(agc_split_table_call_tests EXCLUDE_FROM_ALL tests/SplitTableCall.cpp)
target_link_libraries(agc_split_table_call_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_split_table_call_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_split_table_call_tests)

if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_link_libraries(agc_split_table_call_tests PRIVATE SPIRV-Tools-static)
endif()

if(BUILD_TESTING)
    agc_add_spirv_mode_tests(agc_split_table_call_tests)
    add_test(NAME agc_driver_split_table_call_run_bindless COMMAND agc_driver_split_table_call_run_tests --bindless)
    set_tests_properties(agc_driver_split_table_call_run_bindless PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PIPELINE_JOURNAL=1;APS5_BINDLESS_IMAGES=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-|was not found but was requested by env var VK_INSTANCE_LAYERS")
    if(WIN32)
        set_tests_properties(agc_driver_split_table_call_run_bindless PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
