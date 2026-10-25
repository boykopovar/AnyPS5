add_library(libSceRudp SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/Time/TimedWait.cpp
)
target_include_directories(libSceRudp PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceRudp)
