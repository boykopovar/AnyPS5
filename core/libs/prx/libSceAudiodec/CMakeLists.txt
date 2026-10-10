include(${CMAKE_CURRENT_SOURCE_DIR}/AudiodecLibrary.cmake)
add_sce_audiodec_library(libSceAudiodec)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_audiodec_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAudiodec.cpp
        LINK libSceAudiodec libc
    )
endif()
