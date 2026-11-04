if(BUILD_TESTING)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test_executable(agc_driver_frame_replay_tests tests/FrameReplay.cpp Execution/src/FrameCapture.cpp)
        target_include_directories(agc_driver_frame_replay_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(agc_driver_frame_replay_tests PRIVATE libSceAgc libSceAgcDriver libkernel libc libzstd_static)
        set_target_properties(agc_driver_frame_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(agc_driver_frame_replay_tests)
        add_dependencies(agc_driver_frame_replay_tests agc_frame_replay)
        add_test(NAME agc_driver_frame_replay COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay>)
        set_tests_properties(agc_driver_frame_replay PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-replay-capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=2;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 120)
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        add_test(NAME agc_driver_frame_replay_concurrent COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay> concurrent)
        set_tests_properties(agc_driver_frame_replay_concurrent PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-replay-concurrent-capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=8;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 120)
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay_concurrent PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
endif()
