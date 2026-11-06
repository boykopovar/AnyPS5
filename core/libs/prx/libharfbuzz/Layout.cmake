if(BUILD_TESTING)
    add_test_executable(guest_harfbuzz_layout_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzLayout.cpp)
    target_include_directories(guest_harfbuzz_layout_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_layout_tests PRIVATE harfbuzz_backend ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_harfbuzz_layout_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_harfbuzz_layout_tests)
    set(harfbuzzLayoutModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_layout_modules)
    set(harfbuzzLayoutModule ${harfbuzzLayoutModuleDir}/libharfbuzz.prx)
    set(harfbuzzLayoutKernel ${harfbuzzLayoutModuleDir}/libkernel.prx)
    set(harfbuzzLayoutLibc ${harfbuzzLayoutModuleDir}/libc.prx)
    set(harfbuzzLayoutExecutable ${harfbuzzLayoutModuleDir}/guest_harfbuzz_layout_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(
        OUTPUT ${harfbuzzLayoutModule} ${harfbuzzLayoutKernel} ${harfbuzzLayoutLibc}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzLayoutModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzLayoutModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzLayoutKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzLayoutLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzLayoutModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzLayoutKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzLayoutLibc}
        DEPENDS nid_patcher libharfbuzz libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_harfbuzz_layout_libraries DEPENDS ${harfbuzzLayoutModule} ${harfbuzzLayoutKernel} ${harfbuzzLayoutLibc})
    add_dependencies(guest_harfbuzz_layout_tests guest_harfbuzz_layout_libraries)
    add_custom_command(TARGET guest_harfbuzz_layout_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_layout_tests> ${harfbuzzLayoutExecutable}
        COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzLayoutExecutable}
        BYPRODUCTS ${harfbuzzLayoutExecutable}
        VERBATIM)
    add_test(NAME guest_harfbuzz_layout COMMAND guest_harfbuzz_layout_tests $<TARGET_FILE:libharfbuzz> unpatched $<TARGET_FILE:libkernel>)
    add_test(NAME guest_harfbuzz_layout_nid COMMAND ${harfbuzzLayoutExecutable} ${harfbuzzLayoutModule} patched ${harfbuzzLayoutKernel})
    set_tests_properties(guest_harfbuzz_layout guest_harfbuzz_layout_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz_layout PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        set_tests_properties(guest_harfbuzz_layout_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_layout_tests>;PATH=path_list_prepend:${harfbuzzLayoutModuleDir}")
    elseif(APPLE)
        set_tests_properties(guest_harfbuzz_layout PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_layout_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_layout_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzLayoutModuleDir}")
    else()
        set_tests_properties(guest_harfbuzz_layout PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_layout_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_layout_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzLayoutModuleDir}")
    endif()
endif()
