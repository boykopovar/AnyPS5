add_library(libSceXml SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DocumentBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DomParser.cpp
)
target_include_directories(libSceXml PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceXml)

if(BUILD_TESTING)
    add_test_executable(xml_dom_parser_tests ${LIBS_INCLUDE_DIR}/tests/XmlDomParser.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/DomParser.cpp)
    target_include_directories(xml_dom_parser_tests PRIVATE ${LIBS_INCLUDE_DIR})
    configure_host_test_executable(xml_dom_parser_tests)
    add_test(NAME xml_dom_parser COMMAND xml_dom_parser_tests)
    add_test_executable(guest_xml_document_builder_tests ${LIBS_INCLUDE_DIR}/tests/GuestXmlDocumentBuilder.cpp)
    target_include_directories(guest_xml_document_builder_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_xml_document_builder_tests PRIVATE libSceXml libkernel libc)
    configure_windows_unwind(guest_xml_document_builder_tests)
    add_test(NAME guest_xml_document_builder COMMAND guest_xml_document_builder_tests)
    set_tests_properties(guest_xml_document_builder PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_xml_document_builder PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceXml>")
    endif()
endif()
