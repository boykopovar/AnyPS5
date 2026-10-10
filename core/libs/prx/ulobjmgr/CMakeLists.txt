add_library(ulobjmgr SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(ulobjmgr PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(ulobjmgr)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_ulobjmgr_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUlobjmgr.cpp
        LINK ulobjmgr libc
    )
endif()
