include(${CMAKE_CURRENT_SOURCE_DIR}/AvPlayerLibrary.cmake)
add_sce_avplayer_library(libSceAvPlayer)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_avplayer_tests
        TIMEOUT 60
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAvPlayer.cpp
        LINK libSceAvPlayer libc
    )
endif()
