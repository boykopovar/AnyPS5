add_library(libSceSsl SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSsl PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSsl)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_ssl_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSsl.cpp
        LINK libSceSsl libc
    )
endif()
