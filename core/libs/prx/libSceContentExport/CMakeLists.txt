add_library(libSceContentExport SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceContentExport PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceContentExport)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_content_export_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestContentExport.cpp
        LINK libSceContentExport libc
        RUNTIME libc
    )
endif()
