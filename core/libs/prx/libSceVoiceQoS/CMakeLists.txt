add_library(libSceVoiceQoS SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceVoiceQoS PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVoiceQoS)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_voice_qos_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestVoiceQoS.cpp
        LINK libSceVoiceQoS libc
        ARGS 0x20000000
    )
    add_test(NAME guest_voice_qos_app_type_0x10000000 COMMAND guest_voice_qos_tests 0x10000000)
    anyps5_set_test_properties(guest_voice_qos_app_type_0x10000000 integration "" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(guest_voice_qos_app_type_0x10000000 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVoiceQoS>")
    endif()
endif()
