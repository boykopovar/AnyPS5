add_library(libSceSaveData.native SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSaveData.native PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSaveData.native)

if(BUILD_TESTING)
    add_test_executable(guest_savedata_search_tests ${LIBS_INCLUDE_DIR}/tests/GuestSaveDataSearch.cpp)
    target_include_directories(guest_savedata_search_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_savedata_search_tests PRIVATE libSceSaveData.native libc)
    configure_windows_unwind(guest_savedata_search_tests)
    add_test(NAME guest_savedata_search COMMAND guest_savedata_search_tests)
    if(WIN32)
        set_tests_properties(guest_savedata_search PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData.native>")
    endif()

    add_test_executable(guest_savedata_delete_tests ${LIBS_INCLUDE_DIR}/tests/GuestSaveDataDelete.cpp)
    target_include_directories(guest_savedata_delete_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_savedata_delete_tests PRIVATE libSceSaveData.native libc)
    add_test(NAME guest_savedata_delete COMMAND guest_savedata_delete_tests)
    if(WIN32)
        set_tests_properties(guest_savedata_delete PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData.native>")
    endif()

    add_test_executable(guest_savedata_ps4_tests ${LIBS_INCLUDE_DIR}/tests/GuestSaveDataPs4.cpp)
    target_include_directories(guest_savedata_ps4_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_savedata_ps4_tests PRIVATE libSceSaveData.native libc)
    configure_windows_unwind(guest_savedata_ps4_tests)
    add_test(NAME guest_savedata_ps4 COMMAND guest_savedata_ps4_tests)
    if(WIN32)
        set_tests_properties(guest_savedata_ps4 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceSaveData.native>")
    endif()
endif()
