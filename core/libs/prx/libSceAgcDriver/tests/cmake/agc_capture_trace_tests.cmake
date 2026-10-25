if(BUILD_TESTING)
    add_test_executable(agc_capture_trace_tests tests/CaptureTrace.cpp)
    target_include_directories(agc_capture_trace_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_capture_trace_tests PRIVATE libc)
    configure_windows_unwind(agc_capture_trace_tests)
    set(agcCaptureTraceDirectory ${CMAKE_CURRENT_BINARY_DIR}/capture_trace_tests)
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/default")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/explicit")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/failure")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/best-effort")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/format")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/queue")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/queue-shutdown")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/queue-cumulative")
    file(MAKE_DIRECTORY "${agcCaptureTraceDirectory}/disabled")
    set(agcCaptureTraceExplicitPath "${agcCaptureTraceDirectory}/explicit/trace.log")
    set(agcCaptureTraceFailurePath "${agcCaptureTraceDirectory}/failure/missing-parent/trace.log")
    set(agcCaptureTraceBestEffortPath "${agcCaptureTraceDirectory}/best-effort/trace.log")
    set(agcCaptureTraceFormatPath "${agcCaptureTraceDirectory}/format/trace.log")
    set(agcCaptureTraceQueuePath "${agcCaptureTraceDirectory}/queue/trace.log")
    set(agcCaptureTraceQueueShutdownPath "${agcCaptureTraceDirectory}/queue-shutdown/trace.log")
    set(agcCaptureTraceQueueCumulativePath "${agcCaptureTraceDirectory}/queue-cumulative/trace.log")
    add_test(NAME agc_capture_trace_path_default COMMAND agc_capture_trace_tests path-default)
    add_test(NAME agc_capture_trace_path_empty COMMAND agc_capture_trace_tests path-empty)
    set_tests_properties(agc_capture_trace_path_empty PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE_PATH=")
    add_test(NAME agc_capture_trace_path_explicit COMMAND agc_capture_trace_tests path-explicit)
    set_tests_properties(agc_capture_trace_path_explicit PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceExplicitPath}")
    add_test(NAME agc_capture_trace_explicit_writes COMMAND agc_capture_trace_tests explicit-writes)
    set_tests_properties(agc_capture_trace_explicit_writes PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceExplicitPath}")
    add_test(NAME agc_capture_trace_default_writes COMMAND agc_capture_trace_tests default-writes)
    set_tests_properties(agc_capture_trace_default_writes PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/default")
    add_test(NAME agc_capture_trace_open_failure COMMAND agc_capture_trace_tests open-failure)
    set_tests_properties(agc_capture_trace_open_failure PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceFailurePath}" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/failure")
    add_test(NAME agc_capture_trace_best_effort_overflow COMMAND agc_capture_trace_tests best-effort-overflow)
    set_tests_properties(agc_capture_trace_best_effort_overflow PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceBestEffortPath}" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/best-effort")
    add_test(NAME agc_capture_trace_format_overflow COMMAND agc_capture_trace_tests format-overflow)
    set_tests_properties(agc_capture_trace_format_overflow PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceFormatPath}" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/format" TIMEOUT 15)
    add_test(NAME agc_capture_trace_queue_drop COMMAND agc_capture_trace_tests queue-drop)
    set_tests_properties(agc_capture_trace_queue_drop PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceQueuePath}" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/queue" TIMEOUT 15)
    add_test(NAME agc_capture_trace_queue_drop_shutdown COMMAND agc_capture_trace_tests queue-drop-shutdown)
    set_tests_properties(agc_capture_trace_queue_drop_shutdown PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceQueueShutdownPath}" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/queue-shutdown" TIMEOUT 15)
    add_test(NAME agc_capture_trace_queue_drop_cumulative COMMAND agc_capture_trace_tests queue-drop-cumulative)
    set_tests_properties(agc_capture_trace_queue_drop_cumulative PROPERTIES ENVIRONMENT "APS5_CAPTURE_TRACE=1;APS5_CAPTURE_TRACE_PATH=${agcCaptureTraceQueueCumulativePath}" WORKING_DIRECTORY "${agcCaptureTraceDirectory}/queue-cumulative" TIMEOUT 15)
    add_test(NAME agc_capture_trace_disabled COMMAND agc_capture_trace_tests disabled)
    set_tests_properties(agc_capture_trace_disabled PROPERTIES WORKING_DIRECTORY "${agcCaptureTraceDirectory}/disabled")
    if(WIN32)
        set_tests_properties(agc_capture_trace_path_default agc_capture_trace_path_empty
                agc_capture_trace_path_explicit agc_capture_trace_explicit_writes
                agc_capture_trace_default_writes agc_capture_trace_open_failure agc_capture_trace_best_effort_overflow agc_capture_trace_format_overflow agc_capture_trace_queue_drop agc_capture_trace_queue_drop_shutdown agc_capture_trace_queue_drop_cumulative
                agc_capture_trace_disabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(agc_capture_trace_failure_tests tests/CaptureTraceFailure.cpp)
    target_include_directories(agc_capture_trace_failure_tests PRIVATE ${LIBS_INCLUDE_DIR})
    if(MINGW)
        target_compile_options(agc_capture_trace_failure_tests PRIVATE -fasynchronous-unwind-tables)
    endif()
    add_test_executable(agc_capture_trace_runtime_failure_tests tests/CaptureTraceFailure.cpp)
    target_include_directories(agc_capture_trace_runtime_failure_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_capture_trace_runtime_failure_tests PRIVATE libc)
    configure_windows_unwind(agc_capture_trace_runtime_failure_tests)
    foreach(mode IN ITEMS write flush)
        add_test(NAME agc_capture_trace_${mode}_failure
            COMMAND "${CMAKE_COMMAND}"
                "-DFIXTURE=$<TARGET_FILE:agc_capture_trace_failure_tests>"
                "-DMODE=${mode}"
                "-DOUTPUT_ROOT=${agcCaptureTraceDirectory}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/CaptureTraceFailure.cmake")
        set_tests_properties(agc_capture_trace_${mode}_failure PROPERTIES TIMEOUT 20)
        add_test(NAME agc_capture_trace_${mode}_failure_runtime
            COMMAND "${CMAKE_COMMAND}"
                "-DFIXTURE=$<TARGET_FILE:agc_capture_trace_runtime_failure_tests>"
                "-DMODE=${mode}"
                "-DOUTPUT_ROOT=${agcCaptureTraceDirectory}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/CaptureTraceFailure.cmake")
        set_tests_properties(agc_capture_trace_${mode}_failure_runtime PROPERTIES TIMEOUT 20)
        if(WIN32)
            set_tests_properties(agc_capture_trace_${mode}_failure_runtime PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
