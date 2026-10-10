add_library(libSceJson2 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceJson2 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceJson2)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_json_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestJson.cpp
        LINK libSceJson2 libc
    )
endif()
