add_library(libScePad SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PadState.cpp
)
target_include_directories(libScePad PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePad PROPERTIES
    CXX_EXTENSIONS OFF
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
configure_windows_unwind(libScePad)
