add_library(libSceGameUpdate SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceGameUpdate PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceGameUpdate)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_gameupdate_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestGameUpdate.cpp
        LINK libSceGameUpdate libc
    )
endif()
