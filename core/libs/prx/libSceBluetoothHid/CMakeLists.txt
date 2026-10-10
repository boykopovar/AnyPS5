add_library(libSceBluetoothHid SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceBluetoothHid PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceBluetoothHid)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_bluetooth_hid_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestBluetoothHid.cpp
        LINK libSceBluetoothHid libc
    )
endif()
