add_library(libScePsml_debug SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libScePsml_debug PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libScePsml_debug)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_psml_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPsml.cpp
        LINK libScePsml_debug libc
    )
endif()
