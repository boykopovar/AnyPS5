add_library(libSceVideodec2 SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVideodec2 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVideodec2)

target_link_libraries(libSceVideodec2 PRIVATE ffmpeg)

if(BUILD_TESTING)
    add_test_executable(guest_videodec2_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideodec2.cpp)
    target_include_directories(guest_videodec2_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_videodec2_tests PRIVATE libSceVideodec2 libc)
    configure_windows_unwind(guest_videodec2_tests)
    add_test(NAME guest_videodec2 COMMAND guest_videodec2_tests)
    if(WIN32)
        set_tests_properties(guest_videodec2 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideodec2>")
    endif()
endif()
