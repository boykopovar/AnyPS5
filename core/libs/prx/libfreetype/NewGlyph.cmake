target_link_libraries(libfreetype PRIVATE libSceFreeType freetype)

if(BUILD_TESTING)
    add_test_executable(guest_legacy_freetype_new_glyph_tests ${LIBS_INCLUDE_DIR}/tests/GuestLegacyFreeTypeNewGlyph.cpp)
    target_include_directories(guest_legacy_freetype_new_glyph_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_legacy_freetype_new_glyph_tests PRIVATE libSceFreeType libkernel freetype libc ${CMAKE_DL_LIBS})
    configure_windows_unwind(guest_legacy_freetype_new_glyph_tests)
    set(legacyFreeTypeNewGlyphPatchedDir ${CMAKE_CURRENT_BINARY_DIR}/guest_legacy_freetype_new_glyph_modules)
    set(legacyFreeTypeNewGlyphPatchedModule ${legacyFreeTypeNewGlyphPatchedDir}/libfreetype.prx)
    add_custom_command(OUTPUT ${legacyFreeTypeNewGlyphPatchedModule}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${legacyFreeTypeNewGlyphPatchedDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libfreetype> ${legacyFreeTypeNewGlyphPatchedModule}
        COMMAND $<TARGET_FILE:nid_patcher> libfreetype --preserve-export-list ${LIBC_EXPORT_LIST} ${legacyFreeTypeNewGlyphPatchedModule}
        DEPENDS nid_patcher libfreetype ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_legacy_freetype_new_glyph_libraries DEPENDS ${legacyFreeTypeNewGlyphPatchedModule})
    add_dependencies(guest_legacy_freetype_new_glyph_tests guest_legacy_freetype_new_glyph_libraries)
    add_test(NAME guest_legacy_freetype_new_glyph COMMAND guest_legacy_freetype_new_glyph_tests $<TARGET_FILE:libfreetype> unpatched)
    add_test(NAME guest_legacy_freetype_new_glyph_nid COMMAND guest_legacy_freetype_new_glyph_tests ${legacyFreeTypeNewGlyphPatchedModule} nid)
    set_tests_properties(guest_legacy_freetype_new_glyph guest_legacy_freetype_new_glyph_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_legacy_freetype_new_glyph guest_legacy_freetype_new_glyph_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libfreetype>")
    endif()
endif()
