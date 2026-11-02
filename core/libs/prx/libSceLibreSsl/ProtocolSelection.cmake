target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/ProtocolSelection.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_protocol_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/GuestLibreSslProtocol.cpp)
    target_include_directories(guest_libressl_protocol_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_protocol_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_libressl_protocol_tests PRIVATE libc)
    endif()
    add_dependencies(guest_libressl_protocol_tests libs)
    configure_windows_unwind(guest_libressl_protocol_tests)
    add_test(NAME guest_libressl_protocol COMMAND guest_libressl_protocol_tests
        $<TARGET_FILE:libSceLibreSsl> SSL_select_next_proto_nid_postfix)
    add_test(NAME guest_libressl_protocol_nid COMMAND guest_libressl_protocol_tests
        ${PATCHED_DIR}/libSceLibreSsl.prx CpIzEs1Q1Ug)
    if(WIN32)
        set_tests_properties(guest_libressl_protocol PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
        set_tests_properties(guest_libressl_protocol_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${PATCHED_DIR}")
    endif()
endif()
