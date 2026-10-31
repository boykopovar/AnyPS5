target_sources(libSceFreeType PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/prx/libSceFreeType/src/PostScriptPrivate.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_postscript_private_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/GuestFreeTypePostScriptPrivate.cpp
    )
    target_include_directories(guest_freetype_postscript_private_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_postscript_private_tests PRIVATE freetype ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_freetype_postscript_private_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_freetype_postscript_private_tests)
    add_dependencies(guest_freetype_postscript_private_tests libs)
    add_test(NAME guest_freetype_postscript_private COMMAND guest_freetype_postscript_private_tests $<TARGET_FILE:libSceFreeType> unpatched)
    add_test(NAME guest_freetype_postscript_private_nid COMMAND guest_freetype_postscript_private_tests ${PATCHED_DIR}/libSceFreeType.prx patched)
    set_tests_properties(guest_freetype_postscript_private guest_freetype_postscript_private_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_freetype_postscript_private PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_postscript_private_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${PATCHED_DIR}")
    endif()
endif()
