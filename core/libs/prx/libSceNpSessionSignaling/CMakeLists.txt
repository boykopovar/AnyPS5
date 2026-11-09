add_library(libSceNpSessionSignaling SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpSessionSignaling PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpSessionSignaling)

if(BUILD_TESTING)
    add_test_executable(guest_np_session_signaling_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpSessionSignaling.cpp)
    target_include_directories(guest_np_session_signaling_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_session_signaling_tests PRIVATE libSceNpSessionSignaling)
    add_test(NAME guest_np_session_signaling COMMAND guest_np_session_signaling_tests)
    if(WIN32)
        set_tests_properties(guest_np_session_signaling PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpSessionSignaling>")
    endif()
endif()
