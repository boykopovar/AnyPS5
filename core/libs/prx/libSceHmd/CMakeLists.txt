add_library(libSceHmd SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceHmd PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceHmd)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_hmd_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestHmd.cpp
        LINK libSceHmd libc
    )
endif()
