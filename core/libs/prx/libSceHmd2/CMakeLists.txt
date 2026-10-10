add_library(libSceHmd2 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceHmd2 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceHmd2)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_hmd2_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestHmd2.cpp
        LINK libSceHmd2 libc
    )
endif()
