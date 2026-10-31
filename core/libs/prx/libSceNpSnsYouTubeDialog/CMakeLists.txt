add_library(libSceNpSnsYouTubeDialog SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpSnsYouTubeDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpSnsYouTubeDialog)

if(BUILD_TESTING)
    add_test_executable(guest_np_sns_youtube_dialog_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpSnsYouTubeDialog.cpp)
    target_include_directories(guest_np_sns_youtube_dialog_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_sns_youtube_dialog_tests PRIVATE libSceNpSnsYouTubeDialog libc)
    add_test(NAME guest_np_sns_youtube_dialog COMMAND guest_np_sns_youtube_dialog_tests)
    if(WIN32)
        set_tests_properties(guest_np_sns_youtube_dialog PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpSnsYouTubeDialog>")
    endif()
endif()
