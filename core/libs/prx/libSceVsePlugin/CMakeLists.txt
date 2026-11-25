add_library(libSceVsePlugin SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libSceVsePlugin PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVsePlugin)

if(BUILD_TESTING)
    add_test_executable(guest_vse_plugin_tests ${LIBS_INCLUDE_DIR}/tests/GuestVsePlugin.cpp)
    target_include_directories(guest_vse_plugin_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vse_plugin_tests PRIVATE libSceVsePlugin libc)
    configure_windows_unwind(guest_vse_plugin_tests)
    add_test(NAME guest_vse_plugin COMMAND guest_vse_plugin_tests)
    if(WIN32)
        set_tests_properties(guest_vse_plugin PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVsePlugin>")
    endif()
endif()
