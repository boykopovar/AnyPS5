add_library(libSceUserService SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceUserService PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceUserService)

if(BUILD_TESTING)
    add_test_executable(guest_user_service_tests ${LIBS_INCLUDE_DIR}/tests/GuestUserService.cpp)
    target_include_directories(guest_user_service_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_user_service_tests PRIVATE libSceUserService)
    add_test(NAME guest_user_service COMMAND guest_user_service_tests)
    if(WIN32)
        set_tests_properties(guest_user_service PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceUserService>")
    endif()
endif()
