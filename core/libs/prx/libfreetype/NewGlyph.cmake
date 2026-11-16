target_link_libraries(libfreetype PRIVATE libSceFreeType freetype)

if(BUILD_TESTING)
    add_test_executable(guest_legacy_freetype_new_glyph_tests ${LIBS_INCLUDE_DIR}/tests/GuestLegacyFreeTypeNewGlyph.cpp)
    target_include_directories(guest_legacy_freetype_new_glyph_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_legacy_freetype_new_glyph_tests PRIVATE libSceFreeType libkernel freetype libc ${CMAKE_DL_LIBS})
    configure_windows_unwind(guest_legacy_freetype_new_glyph_tests)
    set(legacyFreeTypeNewGlyphPatchedDir ${CMAKE_CURRENT_BINARY_DIR}/guest_legacy_freetype_new_glyph_modules/$<CONFIG>)
    set(legacyFreeTypeNewGlyphPatchedModule ${legacyFreeTypeNewGlyphPatchedDir}/libfreetype.prx)
    set(legacyFreeTypeNewGlyphPatchedLibraries)
    foreach(legacyFreeTypeNewGlyphLibrary IN ITEMS libfreetype libSceFreeType libkernel libc)
        set(legacyFreeTypeNewGlyphLibraryOutput ${legacyFreeTypeNewGlyphPatchedDir}/${legacyFreeTypeNewGlyphLibrary}.prx)
        if(legacyFreeTypeNewGlyphLibrary STREQUAL "libc")
            set(legacyFreeTypeNewGlyphPatchArguments libc)
        else()
            set(legacyFreeTypeNewGlyphPatchArguments ${legacyFreeTypeNewGlyphLibrary} --preserve-export-list ${LIBC_EXPORT_LIST})
        endif()
        add_custom_command(OUTPUT ${legacyFreeTypeNewGlyphLibraryOutput}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${legacyFreeTypeNewGlyphPatchedDir}
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${legacyFreeTypeNewGlyphLibrary}> ${legacyFreeTypeNewGlyphLibraryOutput}.tmp
            COMMAND $<TARGET_FILE:nid_patcher> ${legacyFreeTypeNewGlyphPatchArguments} ${legacyFreeTypeNewGlyphLibraryOutput}.tmp
            COMMAND ${CMAKE_COMMAND} -E rename ${legacyFreeTypeNewGlyphLibraryOutput}.tmp ${legacyFreeTypeNewGlyphLibraryOutput}
            DEPENDS nid_patcher ${legacyFreeTypeNewGlyphLibrary} ${LIBC_EXPORT_LIST}
            VERBATIM)
        list(APPEND legacyFreeTypeNewGlyphPatchedLibraries ${legacyFreeTypeNewGlyphLibraryOutput})
    endforeach()
    add_custom_target(guest_legacy_freetype_new_glyph_libraries DEPENDS ${legacyFreeTypeNewGlyphPatchedLibraries})
    add_dependencies(guest_legacy_freetype_new_glyph_tests guest_legacy_freetype_new_glyph_libraries)
    set(legacyFreeTypeNewGlyphPatchedExecutable ${legacyFreeTypeNewGlyphPatchedDir}/guest_legacy_freetype_new_glyph_tests${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(OUTPUT ${legacyFreeTypeNewGlyphPatchedExecutable}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_legacy_freetype_new_glyph_tests> ${legacyFreeTypeNewGlyphPatchedExecutable}.tmp
        COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${legacyFreeTypeNewGlyphPatchedExecutable}.tmp
        COMMAND ${CMAKE_COMMAND} -E rename ${legacyFreeTypeNewGlyphPatchedExecutable}.tmp ${legacyFreeTypeNewGlyphPatchedExecutable}
        DEPENDS guest_legacy_freetype_new_glyph_tests nid_patcher
        VERBATIM)
    add_custom_target(guest_legacy_freetype_new_glyph_fixture ALL DEPENDS ${legacyFreeTypeNewGlyphPatchedExecutable})
    add_test(NAME guest_legacy_freetype_new_glyph COMMAND guest_legacy_freetype_new_glyph_tests $<TARGET_FILE:libfreetype> unpatched)
    add_test(NAME guest_legacy_freetype_new_glyph_nid COMMAND ${legacyFreeTypeNewGlyphPatchedExecutable} ${legacyFreeTypeNewGlyphPatchedModule} nid)
    set_tests_properties(guest_legacy_freetype_new_glyph guest_legacy_freetype_new_glyph_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_legacy_freetype_new_glyph PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libfreetype>")
        set_tests_properties(guest_legacy_freetype_new_glyph_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_legacy_freetype_new_glyph_tests>;PATH=path_list_prepend:${legacyFreeTypeNewGlyphPatchedDir}")
    elseif(APPLE)
        set_tests_properties(guest_legacy_freetype_new_glyph_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${legacyFreeTypeNewGlyphPatchedDir}")
    else()
        set_tests_properties(guest_legacy_freetype_new_glyph_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${legacyFreeTypeNewGlyphPatchedDir}")
    endif()
endif()
