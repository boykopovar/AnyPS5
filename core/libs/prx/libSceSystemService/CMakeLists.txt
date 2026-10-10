add_library(libSceSystemService SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSystemService PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSystemService)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_system_exit_tests
        NAME guest_system_exit_parameters
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSystemExit.cpp
        LINK libSceSystemService libc
        RUNTIME libc
    )
    set(guestSystemExitModes
        "guest_system_exit|LoadExec_Exit_RunsShutdownAndAtexitHandlers|exit|Guest shutdown and atexit completed"
        "guest_quick_exit|QuickExit_TwoHandlers_RunInReverseOrder|quick-exit|Guest quick_exit handlers completed"
        "guest_immediate_exit|Exit_Immediate_SkipsShutdownAndAtexitHandlers|immediate|"
        "guest_return_from_main|CatchReturnFromMain_ZeroStatus_EndsProcessSuccessfully|return-from-main|")
    foreach(exitMode IN LISTS guestSystemExitModes)
        string(REPLACE "|" ";" exitMode "${exitMode}")
        list(GET exitMode 0 exitTestName)
        list(GET exitMode 1 exitCaseName)
        list(GET exitMode 2 exitArgument)
        list(GET exitMode 3 exitOutput)
        add_test(NAME ${exitTestName} COMMAND guest_system_exit_tests --case ${exitCaseName} ${exitArgument})
        set(exitProperties)
        if(exitOutput)
            set(exitProperties PASS_REGULAR_EXPRESSION "${exitOutput}")
        endif()
        anyps5_set_test_properties(${exitTestName} integration "" ${exitProperties})
        if(WIN32)
            set_tests_properties(${exitTestName} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    anyps5_add_library_test(guest_system_service_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSystemService.cpp
        LINK libSceSystemService libc
        RUNTIME libc
    )
    anyps5_add_library_test(guest_system_kill_app_tests
        NAME guest_system_kill_app_parameters
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSystemKillApp.cpp
        LINK libSceSystemService libc
        RUNTIME libc
    )
    foreach(killTestName IN ITEMS guest_system_kill_app guest_system_kill_app_status)
        add_test(NAME ${killTestName} COMMAND guest_system_kill_app_tests
            --case KillApp_RunningTitle_RunsShutdownAndAtexitHandlers kill)
        if(WIN32)
            set_tests_properties(${killTestName} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    anyps5_set_test_properties(guest_system_kill_app integration ""
        PASS_REGULAR_EXPRESSION "Guest shutdown and atexit completed")
    anyps5_set_test_properties(guest_system_kill_app_status integration "")
endif()
