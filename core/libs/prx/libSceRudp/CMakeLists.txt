add_library(libSceRudp SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/Time/TimedWait.cpp
)
target_include_directories(libSceRudp PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceRudp)

if(BUILD_TESTING)
    add_test_executable(guest_rudp_empty_poll_tests ${LIBS_INCLUDE_DIR}/tests/GuestRudpEmptyPoll.cpp)
    target_include_directories(guest_rudp_empty_poll_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_rudp_empty_poll_tests PRIVATE libSceRudp libkernel libc)
    configure_windows_unwind(guest_rudp_empty_poll_tests)
    add_test(NAME guest_rudp_empty_poll COMMAND guest_rudp_empty_poll_tests)
    set_tests_properties(guest_rudp_empty_poll PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_rudp_empty_poll PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceRudp>")
    endif()
endif()
