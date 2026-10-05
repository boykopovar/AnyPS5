add_library(libSceNpTrophy2 SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GameInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrophyInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GroupInfo.cpp
)

target_include_directories(libSceNpTrophy2 PRIVATE
        ${LIBS_INCLUDE_DIR}
)
target_link_libraries(libSceNpTrophy2 PRIVATE libc)
configure_windows_unwind(libSceNpTrophy2)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_nptrophy2_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpTrophy2.cpp)
    target_include_directories(guest_nptrophy2_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_nptrophy2_tests PRIVATE libSceNpTrophy2 libc)
    configure_windows_unwind(guest_nptrophy2_tests)
    add_test(NAME guest_nptrophy2 COMMAND guest_nptrophy2_tests)
    if(WIN32)
        set_tests_properties(guest_nptrophy2 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpTrophy2>")
    endif()
endif()
