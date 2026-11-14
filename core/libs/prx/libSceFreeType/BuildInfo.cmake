target_sources(libSceFreeType PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/BuildInfo.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_build_info_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeBuildInfo.cpp)
    target_include_directories(guest_freetype_build_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_build_info_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_build_info_tests)
    add_test(NAME guest_freetype_build_info COMMAND guest_freetype_build_info_tests)
    if(WIN32)
        set_tests_properties(guest_freetype_build_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
    endif()
endif()
