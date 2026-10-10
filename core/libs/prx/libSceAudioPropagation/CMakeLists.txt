add_library(libSceAudioPropagation SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceAudioPropagation PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAudioPropagation)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_audio_propagation_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAudioPropagation.cpp
        LINK libSceAudioPropagation libc
        RUNTIME libSceAudioPropagation
    )
endif()
