add_library(libSceMoveTracker SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceMoveTracker PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceMoveTracker)

if(BUILD_TESTING)
    add_test_executable(guest_move_tracker_tests ${LIBS_INCLUDE_DIR}/tests/GuestMoveTracker.cpp)
    target_include_directories(guest_move_tracker_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_move_tracker_tests PRIVATE libSceMoveTracker)
    add_test(NAME guest_move_tracker COMMAND guest_move_tracker_tests)
    if(WIN32)
        set_tests_properties(guest_move_tracker PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceMoveTracker>")
    endif()
endif()
