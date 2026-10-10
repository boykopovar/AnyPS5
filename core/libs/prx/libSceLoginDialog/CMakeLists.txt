add_library(libSceLoginDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceLoginDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceLoginDialog)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_login_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestLoginDialog.cpp
        LINK libSceLoginDialog libc
    )
endif()
