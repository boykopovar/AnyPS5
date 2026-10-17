add_library(libSceNpUniversalDataSystem SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/System.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EventPropertyObject.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EventPropertyArray.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UdsEvent.cpp
)

target_include_directories(libSceNpUniversalDataSystem PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(libSceNpUniversalDataSystem PRIVATE libSceNpTrophy2 libc)
configure_windows_unwind(libSceNpUniversalDataSystem)

add_test_executable(np_uds_events_tests EXCLUDE_FROM_ALL
        tests/Events.cpp
        src/Context.cpp
        src/Event.cpp
        src/EventPropertyArray.cpp
        src/EventPropertyObject.cpp
        src/UdsEvent.cpp
)
target_include_directories(np_uds_events_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(np_uds_events_tests PRIVATE libSceNpTrophy2 libc)
configure_windows_unwind(np_uds_events_tests)
if(BUILD_TESTING)
    set_target_properties(np_uds_events_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME np_uds_events COMMAND np_uds_events_tests)
    if(WIN32)
        set_tests_properties(np_uds_events PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpTrophy2>")
    endif()

    add_test_executable(guest_np_trophy_unlocks_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpTrophyUnlocks.cpp)
    target_include_directories(guest_np_trophy_unlocks_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_trophy_unlocks_tests PRIVATE libSceNpUniversalDataSystem libSceNpTrophy2 libc)
    configure_windows_unwind(guest_np_trophy_unlocks_tests)
    add_test(NAME guest_np_trophy_unlocks COMMAND guest_np_trophy_unlocks_tests)
    if(WIN32)
        set_tests_properties(guest_np_trophy_unlocks PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpUniversalDataSystem>")
    endif()

    add_test_executable(guest_np_trophy_data_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpTrophyData.cpp)
    target_include_directories(guest_np_trophy_data_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_trophy_data_tests PRIVATE libSceNpUniversalDataSystem libSceNpTrophy2 libc)
    configure_windows_unwind(guest_np_trophy_data_tests)
    add_test(NAME guest_np_trophy_data COMMAND guest_np_trophy_data_tests)
    if(WIN32)
        set_tests_properties(guest_np_trophy_data PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpUniversalDataSystem>")
    endif()

    add_test_executable(guest_np_trophy_stats_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpTrophyStats.cpp)
    target_include_directories(guest_np_trophy_stats_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_trophy_stats_tests PRIVATE libSceNpUniversalDataSystem libSceNpTrophy2 libc)
    configure_windows_unwind(guest_np_trophy_stats_tests)
    add_test(NAME guest_np_trophy_stats COMMAND guest_np_trophy_stats_tests)
    if(WIN32)
        set_tests_properties(guest_np_trophy_stats PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpUniversalDataSystem>")
    endif()
endif()
