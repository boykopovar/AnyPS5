include(${CMAKE_CURRENT_SOURCE_DIR}/FontLibrary.cmake)
add_sce_font_library(libSceFont)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_font_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestFont.cpp
        LINK libSceFont libSceFontFt libc
    )
    anyps5_add_library_test(guest_system_font_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSystemFont.cpp
        LINK libSceFont libSceFontFt libc
    )
    anyps5_add_library_test(guest_font_writing_line_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestFontWritingLine.cpp
        LINK libSceFont libc
    )
endif()
