add_library(libSceAmpr SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceAmpr PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceAmpr PRIVATE libkernel)
configure_windows_unwind(libSceAmpr)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_ampr_tests
        NAME guest_ampr_markers
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestAmpr.cpp
        LINK libSceAmpr libkernel libc
    )
endif()
