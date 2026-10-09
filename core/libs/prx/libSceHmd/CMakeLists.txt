add_library(libSceHmd SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceHmd PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceHmd)

if(BUILD_TESTING)
    add_test_executable(guest_hmd_tests ${LIBS_INCLUDE_DIR}/tests/GuestHmd.cpp)
    target_include_directories(guest_hmd_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_hmd_tests PRIVATE libSceHmd)
    add_test(NAME guest_hmd COMMAND guest_hmd_tests)
    if(WIN32)
        set_tests_properties(guest_hmd PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceHmd>")
    endif()
endif()
