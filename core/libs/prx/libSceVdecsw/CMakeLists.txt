add_library(libSceVdecsw SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVdecsw PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVdecsw)

target_link_libraries(libSceVdecsw PRIVATE libSceVideodec2)

if(BUILD_TESTING)
    add_test_executable(guest_vdecsw_tests ${LIBS_INCLUDE_DIR}/tests/GuestVdecsw.cpp)
    target_include_directories(guest_vdecsw_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vdecsw_tests PRIVATE libSceVdecsw libSceVideodec2 libc)
    configure_windows_unwind(guest_vdecsw_tests)
    add_test(NAME guest_vdecsw COMMAND guest_vdecsw_tests)
    if(WIN32)
        set_tests_properties(guest_vdecsw PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVdecsw>")
    endif()
endif()
