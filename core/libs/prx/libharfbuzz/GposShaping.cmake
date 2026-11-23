if(BUILD_TESTING)
    add_test_executable(guest_harfbuzz_gpos_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzGpos.cpp)
    target_include_directories(guest_harfbuzz_gpos_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_gpos_tests PRIVATE harfbuzz_backend ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_harfbuzz_gpos_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_harfbuzz_gpos_tests)
    set(harfbuzzGposModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_gpos_modules)
    set(harfbuzzGposModule ${harfbuzzGposModuleDir}/libharfbuzz.prx)
    set(harfbuzzGposKernel ${harfbuzzGposModuleDir}/libkernel.prx)
    set(harfbuzzGposLibc ${harfbuzzGposModuleDir}/libc.prx)
    set(harfbuzzGposExecutable ${harfbuzzGposModuleDir}/guest_harfbuzz_gpos_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(
        OUTPUT ${harfbuzzGposModule} ${harfbuzzGposKernel} ${harfbuzzGposLibc}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzGposModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzGposModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzGposKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzGposLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzGposModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzGposKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzGposLibc}
        DEPENDS nid_patcher libharfbuzz libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_harfbuzz_gpos_libraries DEPENDS ${harfbuzzGposModule} ${harfbuzzGposKernel} ${harfbuzzGposLibc})
    add_dependencies(guest_harfbuzz_gpos_tests guest_harfbuzz_gpos_libraries)
    add_custom_command(TARGET guest_harfbuzz_gpos_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_gpos_tests> ${harfbuzzGposExecutable}
        COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzGposExecutable}
        BYPRODUCTS ${harfbuzzGposExecutable}
        VERBATIM)
    add_test(NAME guest_harfbuzz_gpos COMMAND guest_harfbuzz_gpos_tests $<TARGET_FILE:libharfbuzz> unpatched)
    add_test(NAME guest_harfbuzz_gpos_nid COMMAND ${harfbuzzGposExecutable} ${harfbuzzGposModule} patched)
    set_tests_properties(guest_harfbuzz_gpos guest_harfbuzz_gpos_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz_gpos PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        set_tests_properties(guest_harfbuzz_gpos_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_gpos_tests>;PATH=path_list_prepend:${harfbuzzGposModuleDir}")
    elseif(APPLE)
        set_tests_properties(guest_harfbuzz_gpos PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_gpos_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_gpos_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzGposModuleDir}")
    else()
        set_tests_properties(guest_harfbuzz_gpos PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_gpos_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_gpos_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzGposModuleDir}")
    endif()
endif()
