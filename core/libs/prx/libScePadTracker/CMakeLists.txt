add_library(libScePadTracker SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libScePadTracker PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libScePadTracker)

if(BUILD_TESTING)
    add_test_executable(guest_pad_tracker_tests ${LIBS_INCLUDE_DIR}/tests/GuestPadTracker.cpp)
    target_include_directories(guest_pad_tracker_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_pad_tracker_tests PRIVATE libScePadTracker)
    add_test(NAME guest_pad_tracker COMMAND guest_pad_tracker_tests)
    if(WIN32)
        set_tests_properties(guest_pad_tracker PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePadTracker>")
    endif()
endif()
