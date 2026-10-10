add_library(libSceUserService SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceUserService PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceUserService)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_user_service_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestUserService.cpp
        LINK libSceUserService libc
    )
endif()
