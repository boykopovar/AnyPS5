add_library(libSceOpusDec SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceOpusDec PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceOpusDec PRIVATE ffmpeg)
configure_windows_unwind(libSceOpusDec)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_opus_dec_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestOpusDec.cpp
        LINK libSceOpusDec libc
    )
endif()
