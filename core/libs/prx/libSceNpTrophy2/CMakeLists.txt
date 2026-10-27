add_library(libSceNpTrophy2 SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GameInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrophyInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GroupInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrophyStore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrophyPackage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TitleTrophies.cpp
)

target_include_directories(libSceNpTrophy2 PRIVATE
        ${LIBS_INCLUDE_DIR}
)
target_link_libraries(libSceNpTrophy2 PRIVATE libc)
configure_windows_unwind(libSceNpTrophy2)

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

add_test_executable(np_trophy2_store_tests EXCLUDE_FROM_ALL tests/TrophyStore.cpp)
target_include_directories(np_trophy2_store_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(np_trophy2_store_tests PRIVATE libSceNpTrophy2 libc)
configure_windows_unwind(np_trophy2_store_tests)
if(BUILD_TESTING)
    set_target_properties(np_trophy2_store_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME np_trophy2_store COMMAND np_trophy2_store_tests)
    if(WIN32)
        set_tests_properties(np_trophy2_store PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpTrophy2>")
    endif()
endif()

add_test_executable(np_trophy2_package_tests EXCLUDE_FROM_ALL tests/TrophyPackage.cpp)
target_include_directories(np_trophy2_package_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(np_trophy2_package_tests PRIVATE libSceNpTrophy2 libc)
configure_windows_unwind(np_trophy2_package_tests)
if(BUILD_TESTING)
    set_target_properties(np_trophy2_package_tests PROPERTIES EXCLUDE_FROM_ALL FALSE)
    add_test(NAME np_trophy2_package COMMAND np_trophy2_package_tests)
    if(WIN32)
        set_tests_properties(np_trophy2_package PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpTrophy2>")
    endif()
endif()
