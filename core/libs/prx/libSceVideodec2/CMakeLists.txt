add_library(libSceVideodec2 SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVideodec2 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVideodec2)

target_link_libraries(libSceVideodec2 PRIVATE ffmpeg)

if(BUILD_TESTING)
    anyps5_add_library_test(guest_videodec2_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestVideodec2.cpp
        LINK libSceVideodec2 libc
    )
endif()
