add_library(libSceFreeType SHARED EXCLUDE_FROM_ALL
        Export.cpp
        src/GuestObjects.cpp
)
target_include_directories(libSceFreeType PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceFreeType PRIVATE freetype)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceFreeType PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libSceFreeType)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeType.cpp)
    target_include_directories(guest_freetype_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_tests)
    add_test(NAME guest_freetype COMMAND guest_freetype_tests)
    if(WIN32)
        set_tests_properties(guest_freetype PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
    endif()
endif()
