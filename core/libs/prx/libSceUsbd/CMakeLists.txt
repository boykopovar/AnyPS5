add_library(libSceUsbd SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceUsbd PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceUsbd)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_usbd_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUsbd.cpp
        LINK libSceUsbd libc
    )
endif()
