add_png_decoder(decoder_png libc)

add_library(libScePngDec SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libScePngDec PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libScePngDec PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libScePngDec)
target_link_libraries(libScePngDec PRIVATE decoder_png)

if(BUILD_TESTING)
    add_test_executable(guest_png_dec_tests ${LIBS_INCLUDE_DIR}/tests/GuestPngDec.cpp)
    target_include_directories(guest_png_dec_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_png_dec_tests PRIVATE libScePngDec decoder_png libc)
    configure_windows_unwind(guest_png_dec_tests)
    add_test(NAME guest_png_dec COMMAND guest_png_dec_tests)
    if(WIN32)
        set_tests_properties(guest_png_dec PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePngDec>")
    endif()
endif()
