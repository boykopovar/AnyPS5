add_library(libSceSsl SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSsl PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSsl)

if(BUILD_TESTING)
    add_test_executable(guest_ssl_tests ${LIBS_INCLUDE_DIR}/tests/GuestSsl.cpp)
    target_include_directories(guest_ssl_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ssl_tests PRIVATE libSceSsl)
    add_test(NAME guest_ssl COMMAND guest_ssl_tests)
    if(WIN32)
        set_tests_properties(guest_ssl PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSsl>")
    endif()
endif()
