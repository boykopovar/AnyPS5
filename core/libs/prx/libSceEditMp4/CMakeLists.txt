add_library(libSceEditMp4 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceEditMp4 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceEditMp4)

if(BUILD_TESTING)
    add_test_executable(guest_edit_mp4_tests ${LIBS_INCLUDE_DIR}/tests/GuestEditMp4.cpp)
    target_include_directories(guest_edit_mp4_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_edit_mp4_tests PRIVATE libSceEditMp4)
    add_test(NAME guest_edit_mp4 COMMAND guest_edit_mp4_tests)
    if(WIN32)
        set_tests_properties(guest_edit_mp4 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceEditMp4>")
    endif()
endif()
