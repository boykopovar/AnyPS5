add_library(libSceVideoStreamingEngineAudioOnly SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVideoStreamingEngineAudioOnly PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVideoStreamingEngineAudioOnly)

if(BUILD_TESTING)
    add_test_executable(guest_vse_audio_only_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoStreamingEngineAudioOnly.cpp)
    target_include_directories(guest_vse_audio_only_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vse_audio_only_tests PRIVATE libSceVideoStreamingEngineAudioOnly libc)
    configure_windows_unwind(guest_vse_audio_only_tests)
    add_test(NAME guest_vse_audio_only COMMAND guest_vse_audio_only_tests)
    if(WIN32)
        set_tests_properties(guest_vse_audio_only PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideoStreamingEngineAudioOnly>")
    endif()
endif()
