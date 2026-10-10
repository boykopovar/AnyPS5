add_library(libSceSysmodule SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSysmodule PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSysmodule)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_sysmodule_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSysmodule.cpp
        LINK libSceSysmodule libc
    )
endif()
