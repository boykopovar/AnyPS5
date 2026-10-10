add_library(libSceSaveDataDialog.native SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSaveDataDialog.native PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSaveDataDialog.native)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_savedata_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSaveDataDialog.cpp
        LINK libSceSaveDataDialog.native libc
    )
endif()
