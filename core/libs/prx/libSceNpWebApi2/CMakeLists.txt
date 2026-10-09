add_library(libSceNpWebApi2 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpWebApi2 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpWebApi2)

if(BUILD_TESTING)
    add_test_executable(guest_np_web_api2_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpWebApi2.cpp)
    target_include_directories(guest_np_web_api2_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_web_api2_tests PRIVATE libSceNpWebApi2)
    add_test(NAME guest_np_web_api2 COMMAND guest_np_web_api2_tests)
    if(WIN32)
        set_tests_properties(guest_np_web_api2 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpWebApi2>")
    endif()
endif()
