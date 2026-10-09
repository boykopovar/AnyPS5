add_library(libSceNpPartner001 SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpPartner001 PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpPartner001)

if(BUILD_TESTING)
    add_test_executable(guest_np_partner001_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpPartner001.cpp)
    target_include_directories(guest_np_partner001_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_partner001_tests PRIVATE libSceNpPartner001)
    add_test(NAME guest_np_partner001 COMMAND guest_np_partner001_tests)
    if(WIN32)
        set_tests_properties(guest_np_partner001 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpPartner001>")
    endif()
endif()
