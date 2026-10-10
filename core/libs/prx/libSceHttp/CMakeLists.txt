add_library(libSceHttp SHARED EXCLUDE_FROM_ALL Export.cpp src/Parse.cpp src/Uri.cpp)
target_include_directories(libSceHttp PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceHttp)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_http_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestHttp.cpp
        LINK libSceHttp libc
    )
    anyps5_add_library_test(guest_http_response_header_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestHttpResponseHeader.cpp
        LINK libSceHttp libc
    )
endif()
