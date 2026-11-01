if(BUILD_TESTING AND WIN32)
    add_test_executable(agc_driver_frame_replay_relaunch_tests tests/FrameReplayRelaunch.cpp Execution/src/FrameCapture.cpp)
    target_include_directories(agc_driver_frame_replay_relaunch_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(agc_driver_frame_replay_relaunch_tests PRIVATE libSceAgcDriver libkernel libc libzstd_static)
    target_link_options(agc_driver_frame_replay_relaunch_tests PRIVATE -Wl,--disable-high-entropy-va)
    set_target_properties(agc_driver_frame_replay_relaunch_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_frame_replay_relaunch_tests)
    add_test(NAME agc_driver_frame_replay_relaunch COMMAND agc_driver_frame_replay_relaunch_tests "${CMAKE_CURRENT_BINARY_DIR}/frame replay relaunch")
    set_tests_properties(agc_driver_frame_replay_relaunch PROPERTIES
            TIMEOUT 120 SKIP_RETURN_CODE 77
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
endif()
