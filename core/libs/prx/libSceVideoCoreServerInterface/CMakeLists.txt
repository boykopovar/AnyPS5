add_library(libSceVideoCoreServerInterface SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVideoCoreServerInterface PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVideoCoreServerInterface)

if(BUILD_TESTING)
    add_test_executable(guest_video_core_server_interface_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoCoreServerInterface.cpp)
    target_include_directories(guest_video_core_server_interface_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_core_server_interface_tests PRIVATE libSceVideoCoreServerInterface libc ${CMAKE_DL_LIBS})
    configure_windows_unwind(guest_video_core_server_interface_tests)
    add_test(NAME guest_video_core_server_interface COMMAND guest_video_core_server_interface_tests)
    if(WIN32)
        set_tests_properties(guest_video_core_server_interface PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideoCoreServerInterface>")
    endif()
endif()
