if(BUILD_TESTING)
    add_test_executable(agc_compare_hook_tests
            tests/CompareHook.cpp
            Execution/src/GuestMemory.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_compare_hook_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_compare_hook_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_compare_hook_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_compare_hook_tests)

    foreach(mode IN ITEMS unhooked installed cleared other-thread cleared-elsewhere)
        add_test(NAME agc_compare_hook_${mode} COMMAND agc_compare_hook_tests ${mode})
        set_tests_properties(agc_compare_hook_${mode} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(agc_compare_hook_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
