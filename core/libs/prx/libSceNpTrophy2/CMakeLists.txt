add_library(libSceNpTrophy2 SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GameInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TrophyInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GroupInfo.cpp
)

target_include_directories(libSceNpTrophy2 PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(libSceNpTrophy2 PRIVATE libc)
