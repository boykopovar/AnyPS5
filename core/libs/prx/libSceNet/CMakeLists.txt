add_library(libSceNet SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNet PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNet)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_sce_net_tests
        TIMEOUT 20
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSceNet.cpp
        LINK libSceNet libSceNetCtl libkernel libc
    )
endif()
