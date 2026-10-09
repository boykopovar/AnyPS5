add_library(libSceContentExport SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceContentExport PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceContentExport PRIVATE decoder_png decoder_jpeg)
target_include_directories(libSceContentExport SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
configure_windows_unwind(libSceContentExport)

if(BUILD_TESTING)
    add_test_executable(guest_content_export_tests ${LIBS_INCLUDE_DIR}/tests/GuestContentExport.cpp)
    target_include_directories(guest_content_export_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_content_export_tests PRIVATE libSceContentExport decoder_jpeg libc)
    configure_windows_unwind(guest_content_export_tests)
    add_test(NAME guest_content_export COMMAND guest_content_export_tests)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_content_export)
    set_tests_properties(guest_content_export PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_content_export
        TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_content_export PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
