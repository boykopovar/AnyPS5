add_library(libSceKbEmulate SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSceKeyboard/PadEmulate.cpp
)
target_include_directories(libSceKbEmulate PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceKbEmulate)
