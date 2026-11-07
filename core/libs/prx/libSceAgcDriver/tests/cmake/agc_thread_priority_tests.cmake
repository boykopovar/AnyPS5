add_test_executable(agc_thread_priority_tests EXCLUDE_FROM_ALL
        tests/ThreadPriority.cpp
        Execution/src/ThreadPriority.cpp
)
target_include_directories(agc_thread_priority_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_thread_priority_tests PRIVATE libc ${CMAKE_DL_LIBS})
set_target_properties(agc_thread_priority_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_thread_priority_tests)

if(BUILD_TESTING)
    set_target_properties(agc_thread_priority_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_thread_priority COMMAND agc_thread_priority_tests)
    if(WIN32)
        set_tests_properties(agc_thread_priority PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        foreach(mode rt high)
            add_test(NAME agc_thread_priority_refused_${mode} COMMAND agc_thread_priority_tests refused)
            set_tests_properties(agc_thread_priority_refused_${mode} PROPERTIES ENVIRONMENT
                "APS5_THREAD_PRIORITY=${mode};DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/aps5-no-rtkit")
        endforeach()
    endif()
endif()
