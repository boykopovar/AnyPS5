include(${CMAKE_CURRENT_SOURCE_DIR}/FontFtLibrary.cmake)
add_sce_font_ft_library(libSceFontFt)

if(BUILD_TESTING)
    add_test_executable(guest_font_ft_support_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontFtSupport.cpp)
    target_include_directories(guest_font_ft_support_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_ft_support_tests PRIVATE libSceFontFt)
    add_test(NAME guest_font_ft_support COMMAND guest_font_ft_support_tests)
    if(WIN32)
        set_tests_properties(guest_font_ft_support PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFontFt>")
    endif()
endif()
