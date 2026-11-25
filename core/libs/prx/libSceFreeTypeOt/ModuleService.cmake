target_sources(libSceFreeTypeOt PRIVATE src/ModuleService.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_ot_module_service_tests NO_RUNTIME_DLL_STAGING
            ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeOtModuleService.cpp)
    target_include_directories(guest_freetype_ot_module_service_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_ot_module_service_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_freetype_ot_module_service_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_freetype_ot_module_service_tests)
    add_custom_target(guest_freetype_ot_module_service_libraries DEPENDS
            libSceFreeTypeOt ${PATCHED_DIR}/libSceFreeTypeOt.prx ${PATCHED_DIR}/libSceFreeType.prx
            ${PATCHED_DIR}/libkernel.prx ${LIBC_PATCHED})
    add_dependencies(guest_freetype_ot_module_service_tests guest_freetype_ot_module_service_libraries)
    add_test(NAME guest_freetype_ot_module_service COMMAND guest_freetype_ot_module_service_tests $<TARGET_FILE:libSceFreeTypeOt> raw)
    add_test(NAME guest_freetype_ot_module_service_patched COMMAND guest_freetype_ot_module_service_tests ${PATCHED_DIR}/libSceFreeTypeOt.prx patched)
    set_tests_properties(guest_freetype_ot_module_service guest_freetype_ot_module_service_patched PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_freetype_ot_module_service PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeTypeOt>")
        set_tests_properties(guest_freetype_ot_module_service_patched PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${PATCHED_DIR}")
    endif()
endif()
