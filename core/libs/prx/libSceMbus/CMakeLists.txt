add_library(libSceMbus SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceMbus PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceMbus)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_mbus_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestMbus.cpp
        LINK libSceMbus libc
    )
endif()
