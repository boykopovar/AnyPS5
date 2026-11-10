add_library(libSceFontGs SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)
target_include_directories(libSceFontGs PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceFontGs PRIVATE libSceFont libkernel_sys)
configure_windows_unwind(libSceFontGs)

if(BUILD_TESTING)
    add_test_executable(guest_font_gs_gnm_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontGsGnm.cpp)
    target_include_directories(guest_font_gs_gnm_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_gs_gnm_tests PRIVATE libSceFontGs libSceFont libkernel libc)
    configure_windows_unwind(guest_font_gs_gnm_tests)
    add_test(NAME guest_font_gs_gnm COMMAND guest_font_gs_gnm_tests)
    if(WIN32)
        set_tests_properties(guest_font_gs_gnm PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFontGs>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFont>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>")
    endif()
endif()
