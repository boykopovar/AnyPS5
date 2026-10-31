add_library(libSceFreeTypeOl SHARED EXCLUDE_FROM_ALL Export.cpp ../libSceFreeType/src/OutlineBorders.cpp)
target_include_directories(libSceFreeTypeOl PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceFreeTypeOl)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_outline_border_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeOutlineBorder.cpp)
    target_include_directories(guest_freetype_outline_border_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_outline_border_tests PRIVATE freetype ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_freetype_outline_border_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_freetype_outline_border_tests)
    add_custom_target(freetype_outline_border_patched DEPENDS
        ${PATCHED_DIR}/libSceFreeTypeOl.prx ${PATCHED_DIR}/libSceFreeTypeOt.prx ${PATCHED_DIR}/libkernel.prx ${LIBC_PATCHED})
    add_dependencies(guest_freetype_outline_border_tests freetype_outline_border_patched)
    add_test(NAME guest_freetype_outline_border_reference COMMAND guest_freetype_outline_border_tests reference)
    foreach(module IN ITEMS Ol Ot)
        foreach(border IN ITEMS inside outside)
            add_test(NAME guest_freetype_outline_border_${module}_${border}
                COMMAND guest_freetype_outline_border_tests $<TARGET_FILE:libSceFreeType${module}> ${border} raw)
            add_test(NAME guest_freetype_outline_border_${module}_${border}_patched
                COMMAND guest_freetype_outline_border_tests ${PATCHED_DIR}/libSceFreeType${module}.prx ${border} patched)
            set_tests_properties(guest_freetype_outline_border_${module}_${border}
                guest_freetype_outline_border_${module}_${border}_patched PROPERTIES TIMEOUT 20)
            if(WIN32)
                set_tests_properties(guest_freetype_outline_border_${module}_${border} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeTypeOl>")
                set_tests_properties(guest_freetype_outline_border_${module}_${border}_patched PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:${PATCHED_DIR}")
            endif()
        endforeach()
    endforeach()
    set_tests_properties(guest_freetype_outline_border_reference PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_freetype_outline_border_reference PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeTypeOl>")
    endif()
endif()
