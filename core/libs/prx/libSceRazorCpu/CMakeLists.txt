add_library(libSceRazorCpu SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceRazorCpu PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceRazorCpu)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_razor_cpu_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestRazorCpu.cpp
        LINK libSceRazorCpu libc
    )
endif()
