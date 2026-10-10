add_library(libSceRtc SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceRtc PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceRtc)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_rtc_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestRtc.cpp
        LINK libSceRtc libc
    )
endif()
