add_library(libSceKbEmulate SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSceKeyboard/PadEmulate.cpp
)
target_include_directories(libSceKbEmulate PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceKbEmulate)

if(BUILD_TESTING)
    add_test_executable(guest_kb_emulate_tests ${LIBS_INCLUDE_DIR}/tests/GuestKbEmulate.cpp)
    target_include_directories(guest_kb_emulate_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_kb_emulate_tests PRIVATE libSceKbEmulate libc)
    configure_windows_unwind(guest_kb_emulate_tests)
    add_test(NAME guest_kb_emulate COMMAND guest_kb_emulate_tests)
    if(WIN32)
        set_tests_properties(guest_kb_emulate PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceKbEmulate>")
    endif()
endif()
