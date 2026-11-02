target_sources(libSceFreeType PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/CurveBounds.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_curve_bounds_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeCurveBounds.cpp)
    target_include_directories(guest_freetype_curve_bounds_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_curve_bounds_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_curve_bounds_tests)
    add_test(NAME guest_freetype_curve_bounds COMMAND guest_freetype_curve_bounds_tests)
    if(WIN32)
        set_tests_properties(guest_freetype_curve_bounds PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
    endif()
endif()
