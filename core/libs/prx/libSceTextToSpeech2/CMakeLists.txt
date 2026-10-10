add_library(libSceTextToSpeech2 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceTextToSpeech2 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceTextToSpeech2)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_text_to_speech2_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestTextToSpeech2.cpp
        LINK libSceTextToSpeech2 libc
    )
endif()
