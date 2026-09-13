add_library(libSceAgcDriver SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Eq/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Resource/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Submit/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/State/Export.cpp
)

target_include_directories(libSceAgcDriver PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceAgcDriver PRIVATE libc)
