add_library(libSceMsgDialog.native SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceMsgDialog.native PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceMsgDialog.native)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_msg_dialog_native_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestMsgDialogNative.cpp
        LINK libSceMsgDialog.native libc
    )
endif()
