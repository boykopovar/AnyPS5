add_library(libSceZlib SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceZlib PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src/spng/zlib)
set_target_properties(libSceZlib PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceZlib)
target_link_libraries(libSceZlib PRIVATE jpeg_turbo)

if(BUILD_TESTING)
    add_test_executable(guest_zlib_tests ${LIBS_INCLUDE_DIR}/tests/GuestZlib.cpp)
    target_include_directories(guest_zlib_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_zlib_tests PRIVATE libSceZlib libc)
    configure_windows_unwind(guest_zlib_tests)
    add_test(NAME guest_zlib COMMAND guest_zlib_tests)
    set_tests_properties(guest_zlib PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_zlib PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceZlib>")
    endif()
endif()
