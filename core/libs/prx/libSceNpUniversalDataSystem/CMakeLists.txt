add_library(libSceNpUniversalDataSystem SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/System.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EventPropertyObject.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EventPropertyArray.cpp
)

target_include_directories(libSceNpUniversalDataSystem PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(libSceNpUniversalDataSystem PRIVATE libc)
