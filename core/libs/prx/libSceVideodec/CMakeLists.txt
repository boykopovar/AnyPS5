add_library(libSceVideodec SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVideodec PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVideodec)

target_link_libraries(libSceVideodec PRIVATE libSceVideodec2)

if(BUILD_TESTING)
    add_test_executable(guest_videodec_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideodec.cpp)
    target_include_directories(guest_videodec_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_videodec_tests PRIVATE libSceVideodec libSceVideodec2 libc)
    configure_windows_unwind(guest_videodec_tests)
    add_test(NAME guest_videodec COMMAND guest_videodec_tests)
    if(WIN32)
        set_tests_properties(guest_videodec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideodec>")
    endif()
endif()
