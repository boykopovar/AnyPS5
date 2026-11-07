add_library(libSceFontGs SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)
target_include_directories(libSceFontGs PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceFontGs PRIVATE libSceFont libSceGnmDriver libkernel libkernel_sys mbedcrypto)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceFontGs PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libSceFontGs)

if(BUILD_TESTING)
    add_test_executable(guest_font_gs_gnm_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontGsGnm.cpp)
    target_include_directories(guest_font_gs_gnm_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_font_gs_gnm_tests PRIVATE libSceFontGs libSceFont libkernel libc)
    configure_windows_unwind(guest_font_gs_gnm_tests)
    add_test(NAME guest_font_gs_gnm COMMAND guest_font_gs_gnm_tests)
    add_test(NAME guest_font_gs_gnm_module COMMAND guest_font_gs_gnm_tests --module)
    set_tests_properties(guest_font_gs_gnm_module PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(guest_font_gs_gnm guest_font_gs_gnm_module PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFontGs>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFont>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel_sys>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGnmDriver>")
    endif()
endif()
