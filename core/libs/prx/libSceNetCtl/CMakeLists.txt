add_library(libSceNetCtl SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNetCtl PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNetCtl)

if(BUILD_TESTING)
    add_test_executable(guest_net_ctl_tests ${LIBS_INCLUDE_DIR}/tests/GuestNetCtl.cpp)
    target_include_directories(guest_net_ctl_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_net_ctl_tests PRIVATE libSceNetCtl)
    add_test(NAME guest_net_ctl COMMAND guest_net_ctl_tests)
    if(WIN32)
        set_tests_properties(guest_net_ctl PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNetCtl>")
    endif()
endif()
