add_library(libSceHttp2 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceHttp2 PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceHttp2 PRIVATE libc libkernel)
configure_windows_unwind(libSceHttp2)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_http2_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestHttp2.cpp
        LINK libSceHttp2 libkernel libc
    )
endif()
