if(BUILD_TESTING)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test_executable(agc_driver_frame_replay_tests tests/FrameReplay.cpp Execution/src/FrameCapture.cpp)
        target_include_directories(agc_driver_frame_replay_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR} ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR} $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
        target_link_libraries(agc_driver_frame_replay_tests PRIVATE libSceAgc libSceAgcDriver libkernel libc libzstd_static)
        set_target_properties(agc_driver_frame_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(agc_driver_frame_replay_tests)
        add_dependencies(agc_driver_frame_replay_tests agc_frame_replay)
        add_test(NAME agc_driver_frame_replay COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay>)
        set_tests_properties(agc_driver_frame_replay PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame replay capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=2;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 120 SKIP_RETURN_CODE 77)
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        add_test(NAME agc_driver_frame_replay_cold_pipelines COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay> pipelines)
        set_tests_properties(agc_driver_frame_replay_cold_pipelines PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=0;APS5_HEADLESS_PIPELINE_CACHE=1;ANYPS5_SHADER_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/frame replay pipeline cache;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame replay cold capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=2;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 30 SKIP_RETURN_CODE 77)
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay_cold_pipelines PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        add_test(NAME agc_driver_frame_replay_reservation_error COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay> reservation_error)
        set_tests_properties(agc_driver_frame_replay_reservation_error PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame replay reservation error;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 30 SKIP_RETURN_CODE 77)
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay_reservation_error PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        add_test(NAME agc_driver_frame_replay_concurrent COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay> concurrent)
        set_tests_properties(agc_driver_frame_replay_concurrent PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-replay-concurrent-capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=8;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 120 SKIP_RETURN_CODE 77)
        add_test(NAME agc_driver_frame_replay_rewind COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay> rewind)
        set_tests_properties(agc_driver_frame_replay_rewind PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-replay-rewind-capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=1;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                TIMEOUT 120 SKIP_RETURN_CODE 77)
        if(WIN32)
            set_tests_properties(agc_driver_frame_replay_concurrent agc_driver_frame_replay_rewind PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        foreach(mode IN ITEMS rewind_store rewind_copy rewind_compute rewind_release rewind_fill rewind_split wait_label gpu_store gpu_fill gpu_restore_cached unwatched_start unwatched_late unwatched_shared gpu_cleared fence_reuse busy_queue)
            add_test(NAME agc_driver_frame_replay_${mode} COMMAND agc_driver_frame_replay_tests $<TARGET_FILE:agc_frame_replay> ${mode})
            set_tests_properties(agc_driver_frame_replay_${mode} PROPERTIES
                    ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-replay-${mode}-capture;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=2;APS5_FRAME_CAPTURE_MIN_FREE_BYTES=0"
                    SKIP_RETURN_CODE 77 TIMEOUT 120)
            if(WIN32)
                set_tests_properties(agc_driver_frame_replay_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                        "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endif()
endif()
