add_library(libSceWebBrowserDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceWebBrowserDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceWebBrowserDialog)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_web_browser_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestWebBrowserDialog.cpp
        LINK libSceWebBrowserDialog libc
    )
endif()
