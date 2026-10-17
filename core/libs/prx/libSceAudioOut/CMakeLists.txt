add_library(libSceAudioOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOutSdl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOutShutdown.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2PadMix.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2Port.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2SpeakerArray.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2User.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2Mastering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2System.cpp
)

target_include_directories(libSceAudioOut PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${ANYPS5_SDL2_INCLUDE_DIR}
)
target_link_libraries(libSceAudioOut PRIVATE libc libkernel libScePad SDL2-static)
configure_windows_unwind(libSceAudioOut)

if(BUILD_TESTING)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test_executable(audio_out_port_locks_tests tests/AudioOutPortLocks.cpp)
        target_include_directories(audio_out_port_locks_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
        target_link_libraries(audio_out_port_locks_tests PRIVATE libSceAudioOut libc)
        set_target_properties(audio_out_port_locks_tests PROPERTIES ENABLE_EXPORTS ON)
        add_test(NAME audio_out_independent_output COMMAND audio_out_port_locks_tests output)
        add_test(NAME audio_out_independent_close COMMAND audio_out_port_locks_tests close)
        add_test(NAME audio_out_same_port_close COMMAND audio_out_port_locks_tests same-close)
        set_tests_properties(audio_out_independent_output audio_out_independent_close audio_out_same_port_close PROPERTIES TIMEOUT 10)

        add_test_executable(audio_out_resampling_tests tests/AudioOutResampling.cpp)
        target_include_directories(audio_out_resampling_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
        target_link_libraries(audio_out_resampling_tests PRIVATE libSceAudioOut libc)
        set_target_properties(audio_out_resampling_tests PROPERTIES ENABLE_EXPORTS ON)
        add_test(NAME audio_out_resampling COMMAND audio_out_resampling_tests)
        set_tests_properties(audio_out_resampling PROPERTIES TIMEOUT 10)

    endif()

    add_test_executable(audio_out_initialization_tests tests/AudioOutInitialization.cpp src/AudioOutSdl.cpp)
    target_include_directories(audio_out_initialization_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(audio_out_initialization_tests PRIVATE libc)
    configure_windows_unwind(audio_out_initialization_tests)
    add_test(NAME audio_out_initialization COMMAND audio_out_initialization_tests)
    set_tests_properties(audio_out_initialization PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(audio_out_initialization PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(audio_out2_pad_mix_tests tests/AudioOut2PadMix.cpp src/AudioOut2PadMix.cpp)
    target_include_directories(audio_out2_pad_mix_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_pad_mix_tests PRIVATE libc)
    configure_windows_unwind(audio_out2_pad_mix_tests)
    add_test(NAME audio_out2_pad_mix COMMAND audio_out2_pad_mix_tests)
    if(WIN32)
        set_tests_properties(audio_out2_pad_mix PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(audio_out_mix_level_pad_spk_tests tests/AudioOutMixLevelPadSpk.cpp)
    target_include_directories(audio_out_mix_level_pad_spk_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_mix_level_pad_spk_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out_mix_level_pad_spk_tests)
    add_test(NAME audio_out_mix_level_pad_spk COMMAND audio_out_mix_level_pad_spk_tests)
    if(WIN32)
        set_tests_properties(audio_out_mix_level_pad_spk PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out_last_output_time_tests tests/AudioOutLastOutputTime.cpp)
    target_include_directories(audio_out_last_output_time_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_last_output_time_tests PRIVATE libSceAudioOut libkernel libc)
    configure_windows_unwind(audio_out_last_output_time_tests)
    add_test(NAME audio_out_last_output_time COMMAND audio_out_last_output_time_tests)
    if(WIN32)
        set_tests_properties(audio_out_last_output_time PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out_ex_tests tests/AudioOutEx.cpp)
    target_include_directories(audio_out_ex_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_ex_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out_ex_tests)
    add_test(NAME audio_out_ex COMMAND audio_out_ex_tests)
    if(WIN32)
        set_tests_properties(audio_out_ex PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out_sys_tests tests/AudioOutSys.cpp)
    target_include_directories(audio_out_sys_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_sys_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out_sys_tests)
    add_test(NAME audio_out_sys COMMAND audio_out_sys_tests)
    if(WIN32)
        set_tests_properties(audio_out_sys PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out_sys_hdmi_monitor_info_tests tests/AudioOutSysHdmiMonitorInfo.cpp)
    target_include_directories(audio_out_sys_hdmi_monitor_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_sys_hdmi_monitor_info_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out_sys_hdmi_monitor_info_tests)
    add_test(NAME audio_out_sys_hdmi_monitor_info COMMAND audio_out_sys_hdmi_monitor_info_tests)
    if(WIN32)
        set_tests_properties(audio_out_sys_hdmi_monitor_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out_ex_monitor_info_tests tests/AudioOutExMonitorInfo.cpp)
    target_include_directories(audio_out_ex_monitor_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_ex_monitor_info_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out_ex_monitor_info_tests)
    add_test(NAME audio_out_ex_monitor_info COMMAND audio_out_ex_monitor_info_tests)
    if(WIN32)
        set_tests_properties(audio_out_ex_monitor_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out2_latency_tests tests/AudioOut2Latency.cpp)
    target_include_directories(audio_out2_latency_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_latency_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out2_latency_tests)
    add_test(NAME audio_out2_latency COMMAND audio_out2_latency_tests)
    if(WIN32)
        set_tests_properties(audio_out2_latency PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()

    add_test_executable(audio_out2_port_layouts_tests tests/AudioOut2PortLayouts.cpp)
    target_include_directories(audio_out2_port_layouts_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_port_layouts_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out2_port_layouts_tests)
    add_test(NAME audio_out2_port_layouts COMMAND audio_out2_port_layouts_tests)
    if(WIN32)
        set_tests_properties(audio_out2_port_layouts PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()
    add_test_executable(audio_out2_port_buffers_tests tests/AudioOut2PortBuffers.cpp)
    target_include_directories(audio_out2_port_buffers_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_port_buffers_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out2_port_buffers_tests)
    add_test(NAME audio_out2_port_buffers COMMAND audio_out2_port_buffers_tests)
    if(WIN32)
        set_tests_properties(audio_out2_port_buffers PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()
    add_test_executable(audio_out2_device_state_tests tests/AudioOut2DeviceState.cpp)
    target_include_directories(audio_out2_device_state_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_device_state_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out2_device_state_tests)
    add_test(NAME audio_out2_device_state COMMAND audio_out2_device_state_tests)
    if(WIN32)
        set_tests_properties(audio_out2_device_state PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()
    add_test_executable(audio_out2_timing_tests tests/AudioOut2Timing.cpp)
    target_include_directories(audio_out2_timing_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_timing_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out2_timing_tests)
    add_test(NAME audio_out2_timing COMMAND audio_out2_timing_tests)
    if(WIN32)
        set_tests_properties(audio_out2_timing PROPERTIES RUN_SERIAL TRUE ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()
    add_test_executable(audio_out_shutdown_tests tests/AudioOutShutdown.cpp)
    target_include_directories(audio_out_shutdown_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out_shutdown_tests PRIVATE libSceAudioOut libkernel libc)
    configure_windows_unwind(audio_out_shutdown_tests)
    add_test(NAME audio_out2_shutdown COMMAND audio_out_shutdown_tests --audio-out2)
    add_test(NAME audio_out2_waiting_push_shutdown COMMAND audio_out_shutdown_tests --audio-out2-waiting-push)
    add_test(NAME audio_out2_advance_shutdown COMMAND audio_out_shutdown_tests --audio-out2-advance)
    add_test(NAME audio_out_shutdown COMMAND audio_out_shutdown_tests --audio-out)
    set_tests_properties(audio_out2_shutdown audio_out2_waiting_push_shutdown audio_out2_advance_shutdown audio_out_shutdown PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(audio_out2_shutdown audio_out2_waiting_push_shutdown audio_out2_advance_shutdown audio_out_shutdown PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()
endif()
