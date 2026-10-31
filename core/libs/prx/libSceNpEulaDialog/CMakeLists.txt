add_library(libSceNpEulaDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpEulaDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpEulaDialog)

if(BUILD_TESTING)
    add_test_executable(guest_np_eula_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpEulaDialog.cpp)
    target_include_directories(guest_np_eula_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_eula_dialog_tests PRIVATE libSceNpEulaDialog libc)
    add_test(NAME guest_np_eula_dialog COMMAND guest_np_eula_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_np_eula_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpEulaDialog>")
    endif()
endif()
