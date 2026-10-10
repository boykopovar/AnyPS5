add_library(libSceCesCs SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceCesCs PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceCesCs)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_ces_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestCes.cpp
        LINK libSceCesCs libc
    )
endif()
