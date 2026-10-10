add_library(libSceNpSessionSignaling SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpSessionSignaling PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpSessionSignaling)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_np_session_signaling_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestNpSessionSignaling.cpp
        LINK libSceNpSessionSignaling libc
    )
endif()
