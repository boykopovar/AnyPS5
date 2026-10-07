add_library(libSceCesCs SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceCesCs PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceCesCs)

if(BUILD_TESTING)
    add_test_executable(guest_ces_tests ${LIBS_INCLUDE_DIR}/tests/GuestCes.cpp)
    target_include_directories(guest_ces_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ces_tests PRIVATE libSceCesCs)
    add_test(NAME guest_ces COMMAND guest_ces_tests)
    if(WIN32)
        set_tests_properties(guest_ces PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceCesCs>")
    endif()
endif()
