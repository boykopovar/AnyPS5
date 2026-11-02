if(BUILD_TESTING)
    add_test_executable(agc_driver_draw_program_trace_tests
            tests/DrawProgramTrace.cpp
            Execution/src/Driver/Draw/DrawProgramTrace.cpp
    )
    target_include_directories(agc_driver_draw_program_trace_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
    )
    set_target_properties(agc_driver_draw_program_trace_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_draw_program_trace_tests)
    add_test(NAME agc_driver_draw_program_trace COMMAND agc_driver_draw_program_trace_tests)
    set_tests_properties(agc_driver_draw_program_trace PROPERTIES TIMEOUT 20)
    if(MINGW)
        target_link_libraries(agc_driver_draw_program_trace_tests PRIVATE libc)
        set_tests_properties(agc_driver_draw_program_trace PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
