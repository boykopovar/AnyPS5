add_library(libSceShare SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceShare PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceShare)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_share_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestShare.cpp
        LINK libSceShare libc
    )
endif()
