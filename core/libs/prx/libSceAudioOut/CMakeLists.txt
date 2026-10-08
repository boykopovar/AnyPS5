add_library(libSceAudioOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut.cpp
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
        ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
)
target_link_libraries(libSceAudioOut PRIVATE libc libkernel SDL2-static)
configure_windows_unwind(libSceAudioOut)

if(BUILD_TESTING)
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
    add_test_executable(audio_out2_timing_tests tests/AudioOut2Timing.cpp)
    target_include_directories(audio_out2_timing_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(audio_out2_timing_tests PRIVATE libSceAudioOut libc)
    configure_windows_unwind(audio_out2_timing_tests)
    add_test(NAME audio_out2_timing COMMAND audio_out2_timing_tests)
    if(WIN32)
        set_tests_properties(audio_out2_timing PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAudioOut>")
    endif()
endif()
