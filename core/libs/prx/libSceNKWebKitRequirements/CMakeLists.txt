add_library(libSceNKWebKitRequirements SHARED EXCLUDE_FROM_ALL Export.cpp Zlib.cpp)
target_include_directories(libSceNKWebKitRequirements PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src/spng/zlib)
set_target_properties(libSceNKWebKitRequirements PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceNKWebKitRequirements)
target_link_libraries(libSceNKWebKitRequirements PRIVATE jpeg_turbo)

if(BUILD_TESTING)
    add_test_executable(guest_webkit_zlib_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebKitZlib.cpp)
    target_include_directories(guest_webkit_zlib_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_webkit_zlib_tests PRIVATE libSceNKWebKitRequirements libc)
    configure_windows_unwind(guest_webkit_zlib_tests)
    add_test(NAME guest_webkit_zlib COMMAND guest_webkit_zlib_tests)
    set_tests_properties(guest_webkit_zlib PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_webkit_zlib PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
    endif()
endif()
