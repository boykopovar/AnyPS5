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

if(BUILD_TESTING)
    anyps5_add_library_test(guest_nptrophy2_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestNpTrophy2.cpp
        LINK libSceNpTrophy2 libc
    )
endif()
