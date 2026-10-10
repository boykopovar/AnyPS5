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
    anyps5_add_library_test(audio_out2_pad_mix_tests
        LABEL unit
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/AudioOut2PadMix.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2PadMix.cpp
        LINK libc
    )
    anyps5_add_library_test(audio_out_mix_level_pad_spk_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/AudioOutMixLevelPadSpk.cpp
        LINK libSceAudioOut libc
    )
    anyps5_add_library_test(audio_out_last_output_time_tests
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/AudioOutLastOutputTime.cpp
        LINK libSceAudioOut libkernel libc
    )
    foreach(audioOut2Test IN ITEMS latency:Latency port_layouts:PortLayouts timing:Timing)
        string(REPLACE ":" ";" audioOut2Test "${audioOut2Test}")
        list(GET audioOut2Test 0 audioOut2TestName)
        list(GET audioOut2Test 1 audioOut2TestSource)
        anyps5_add_library_test(audio_out2_${audioOut2TestName}_tests
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/AudioOut2${audioOut2TestSource}.cpp
            LINK libSceAudioOut libc
        )
    endforeach()
endif()
