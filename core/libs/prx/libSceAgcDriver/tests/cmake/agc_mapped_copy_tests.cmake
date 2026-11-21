if(BUILD_TESTING)
    add_test_executable(agc_mapped_copy_tests
            tests/MappedCopy.cpp
            Execution/src/GuestMemory.cpp
            Graphics/src/RuntimeSwitches.cpp
            Execution/src/PerformanceTraceThread.cpp
    )
    target_include_directories(agc_mapped_copy_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_mapped_copy_tests PRIVATE shader_recompiler_agc_driver libc)
    set_target_properties(agc_mapped_copy_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_mapped_copy_tests)

    foreach(mode IN ITEMS valid reset partial reuse boundaries hooks)
        add_test(NAME agc_mapped_copy_${mode} COMMAND agc_mapped_copy_tests ${mode})
        set_tests_properties(agc_mapped_copy_${mode} PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(agc_mapped_copy_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
