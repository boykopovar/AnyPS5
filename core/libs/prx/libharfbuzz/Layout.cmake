if(BUILD_TESTING)
    add_test_executable(guest_harfbuzz_layout_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzLayout.cpp)
    target_include_directories(guest_harfbuzz_layout_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_layout_tests PRIVATE harfbuzz_backend ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_harfbuzz_layout_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_harfbuzz_layout_tests)
    set(harfbuzzLayoutModule ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_layout_modules/libharfbuzz.prx)
    add_custom_command(
        OUTPUT ${harfbuzzLayoutModule}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_layout_modules
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzLayoutModule}
        COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzLayoutModule}
        DEPENDS nid_patcher libharfbuzz ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_harfbuzz_layout_libraries DEPENDS ${harfbuzzLayoutModule})
    add_dependencies(guest_harfbuzz_layout_tests guest_harfbuzz_layout_libraries libkernel)
    add_test(NAME guest_harfbuzz_layout COMMAND guest_harfbuzz_layout_tests $<TARGET_FILE:libharfbuzz> unpatched $<TARGET_FILE:libkernel>)
    add_test(NAME guest_harfbuzz_layout_nid COMMAND guest_harfbuzz_layout_tests ${harfbuzzLayoutModule} patched $<TARGET_FILE:libkernel>)
    set_tests_properties(guest_harfbuzz_layout guest_harfbuzz_layout_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz_layout PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        set_tests_properties(guest_harfbuzz_layout_nid PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${UNPATCHED_DIR}")
    elseif(APPLE)
        set_tests_properties(guest_harfbuzz_layout guest_harfbuzz_layout_nid PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
    else()
        set_tests_properties(guest_harfbuzz_layout guest_harfbuzz_layout_nid PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
    endif()
endif()
