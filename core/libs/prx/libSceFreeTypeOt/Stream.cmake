target_sources(libSceFreeTypeOt PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Stream.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_stream_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeStream.cpp)
    target_include_directories(guest_freetype_stream_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_stream_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_freetype_stream_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_freetype_stream_tests)
    add_custom_target(guest_freetype_stream_libraries DEPENDS
        libSceFreeTypeOt ${PATCHED_DIR}/libSceFreeTypeOt.prx ${PATCHED_DIR}/libkernel.prx ${LIBC_PATCHED})
    add_dependencies(guest_freetype_stream_tests guest_freetype_stream_libraries)
    add_test(NAME guest_freetype_stream COMMAND guest_freetype_stream_tests $<TARGET_FILE:libSceFreeTypeOt> raw)
    add_test(NAME guest_freetype_stream_patched COMMAND guest_freetype_stream_tests ${PATCHED_DIR}/libSceFreeTypeOt.prx patched)
    set_tests_properties(guest_freetype_stream guest_freetype_stream_patched PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_freetype_stream PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeTypeOt>")
        set_tests_properties(guest_freetype_stream_patched PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${PATCHED_DIR}")
    endif()
endif()
