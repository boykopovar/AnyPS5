add_library(libSceFontGs.native SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Surface.cpp
)
target_include_directories(libSceFontGs.native PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceFontGs.native PRIVATE libSceFont)
target_sources(libSceFontGs.native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/AgcBackend.cpp)
configure_windows_unwind(libSceFontGs.native)

if(BUILD_TESTING)
    add_test_executable(guest_font_gs_agc_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontGsAgc.cpp)
    target_include_directories(guest_font_gs_agc_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_gs_agc_tests PRIVATE libSceFontGs.native libSceFont libkernel libc)
    configure_windows_unwind(guest_font_gs_agc_tests)
    add_test(NAME guest_font_gs_agc COMMAND guest_font_gs_agc_tests)
    add_test_executable(guest_font_gs_agc_backend_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontGsAgcBackend.cpp)
    target_include_directories(guest_font_gs_agc_backend_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_gs_agc_backend_tests PRIVATE libSceFontGs.native libSceFont libkernel libc)
    configure_windows_unwind(guest_font_gs_agc_backend_tests)
    add_test(NAME guest_font_gs_agc_backend COMMAND guest_font_gs_agc_backend_tests)
    if(WIN32)
        set_tests_properties(guest_font_gs_agc guest_font_gs_agc_backend PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFontGs.native>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFont>")
    endif()
endif()
