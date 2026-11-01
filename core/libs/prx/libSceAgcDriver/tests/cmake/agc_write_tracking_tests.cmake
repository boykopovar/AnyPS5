add_test_executable(agc_write_tracking_tests EXCLUDE_FROM_ALL
        tests/WriteTracking.cpp
        Execution/src/GuestMemory.cpp
        Execution/src/PerformanceTraceThread.cpp
)
target_include_directories(agc_write_tracking_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_write_tracking_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_write_tracking_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_write_tracking_tests)

if(BUILD_TESTING)
    set_target_properties(agc_write_tracking_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_write_tracking COMMAND agc_write_tracking_tests)
    set_tests_properties(agc_write_tracking PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        add_test(NAME agc_write_tracking_metadata_commit COMMAND agc_write_tracking_tests --metadata-commit)
        set_tests_properties(agc_write_tracking_metadata_commit PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
        add_test(NAME agc_write_tracking_cpu_write_pin COMMAND agc_write_tracking_tests --cpu-write-pin)
        set_tests_properties(agc_write_tracking_cpu_write_pin PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        add_test(NAME agc_resident_direct_remap COMMAND agc_write_tracking_tests --resident-direct-remap)
        set_tests_properties(agc_resident_direct_remap PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        add_test(NAME agc_rewatched_direct_shared COMMAND agc_write_tracking_tests --rewatched-direct-shared)
        set_tests_properties(agc_rewatched_direct_shared PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        add_test(NAME agc_import_alias_shared COMMAND agc_write_tracking_tests --import-alias-shared)
        set_tests_properties(agc_import_alias_shared PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        add_test(NAME agc_compared_shared_generations COMMAND agc_write_tracking_tests --compared-shared-generations)
        set_tests_properties(agc_compared_shared_generations PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        add_test(NAME agc_write_tracking_rewatch_import_pages COMMAND agc_write_tracking_tests --rewatch-import-pages)
        set_tests_properties(agc_write_tracking_rewatch_import_pages PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 20)
        set_tests_properties(agc_write_tracking agc_write_tracking_metadata_commit agc_write_tracking_cpu_write_pin agc_resident_direct_remap agc_rewatched_direct_shared agc_import_alias_shared agc_compared_shared_generations agc_write_tracking_rewatch_import_pages PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
