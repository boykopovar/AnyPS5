if(BUILD_TESTING)
    add_test_executable(agc_driver_pm4_replay_tests tests/Pm4Replay.cpp tools/Pm4Analysis.cpp)
    target_include_directories(agc_driver_pm4_replay_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(agc_driver_pm4_replay_tests PRIVATE libSceAgc libSceAgcDriver libkernel libc)
    set_target_properties(agc_driver_pm4_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_pm4_replay_tests)
    add_dependencies(agc_driver_pm4_replay_tests agc_pm4_replay)
    set(agcPm4ReplayCapture "${CMAKE_CURRENT_BINARY_DIR}/pm4-replay-capture")
    add_test(NAME agc_driver_pm4_replay COMMAND agc_driver_pm4_replay_tests)
    set_tests_properties(agc_driver_pm4_replay PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DUMP_SUBMITS=${agcPm4ReplayCapture}"
            FIXTURES_SETUP agc_pm4_replay_capture TIMEOUT 30)
    add_test(NAME agc_driver_pm4_replay_tool COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:agc_pm4_replay> "-DCAPTURE=${agcPm4ReplayCapture}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/Pm4ReplayTool.cmake)
    set_tests_properties(agc_driver_pm4_replay_tool PROPERTIES
            FIXTURES_REQUIRED agc_pm4_replay_capture TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_pm4_replay agc_driver_pm4_replay_tool PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
