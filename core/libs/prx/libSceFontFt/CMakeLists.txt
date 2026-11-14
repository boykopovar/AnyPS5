include(${CMAKE_CURRENT_SOURCE_DIR}/FontFtLibrary.cmake)
add_sce_font_ft_library(libSceFontFt)

if(BUILD_TESTING)
    add_test_executable(guest_font_ft_backoff_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontFtBackoff.cpp)
    target_include_directories(guest_font_ft_backoff_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_ft_backoff_tests PRIVATE libSceFont libSceFontFt libc)
    configure_windows_unwind(guest_font_ft_backoff_tests)
    add_test(NAME guest_font_ft_backoff COMMAND guest_font_ft_backoff_tests)
    set_tests_properties(guest_font_ft_backoff PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        set_tests_properties(guest_font_ft_backoff PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFontFt>")
    endif()

    add_test_executable(guest_font_ft_init_unwind_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontFtInitUnwind.cpp)
    target_include_directories(guest_font_ft_init_unwind_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_ft_init_unwind_tests PRIVATE libSceFont libSceFontFt libc freetype)
    configure_windows_unwind(guest_font_ft_init_unwind_tests)
    add_test(NAME guest_font_ft_init_unwind_success COMMAND guest_font_ft_init_unwind_tests 0)
    add_test(NAME guest_font_ft_init_unwind_context_null COMMAND guest_font_ft_init_unwind_tests 1)
    add_test(NAME guest_font_ft_init_unwind_context_throw COMMAND guest_font_ft_init_unwind_tests 2)
    add_test(NAME guest_font_ft_init_unwind_memory_null COMMAND guest_font_ft_init_unwind_tests 3)
    add_test(NAME guest_font_ft_init_unwind_memory_throw COMMAND guest_font_ft_init_unwind_tests 4)
    add_test(NAME guest_font_ft_init_unwind_memory_cleanup_throw COMMAND guest_font_ft_init_unwind_tests 5)
    set_tests_properties(guest_font_ft_init_unwind_success guest_font_ft_init_unwind_context_null
        guest_font_ft_init_unwind_context_throw guest_font_ft_init_unwind_memory_null
        guest_font_ft_init_unwind_memory_throw guest_font_ft_init_unwind_memory_cleanup_throw PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_font_ft_init_unwind_success guest_font_ft_init_unwind_context_null
            guest_font_ft_init_unwind_context_throw guest_font_ft_init_unwind_memory_null
            guest_font_ft_init_unwind_memory_throw guest_font_ft_init_unwind_memory_cleanup_throw PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFontFt>")
    endif()
endif()
