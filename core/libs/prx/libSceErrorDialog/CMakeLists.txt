add_library(libSceErrorDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceErrorDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceErrorDialog)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_error_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestErrorDialog.cpp
        LINK libSceErrorDialog libc
    )
endif()
