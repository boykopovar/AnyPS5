add_library(libSceWebTransport SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceWebTransport PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceWebTransport)

if(BUILD_TESTING)
    add_test_executable(guest_web_transport_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebTransport.cpp)
    target_include_directories(guest_web_transport_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_web_transport_tests PRIVATE libSceWebTransport libc)
    configure_windows_unwind(guest_web_transport_tests)
    add_test(NAME guest_web_transport COMMAND guest_web_transport_tests)
    if(WIN32)
        set_tests_properties(guest_web_transport PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceWebTransport>")
    endif()
endif()
