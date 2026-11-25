if(BUILD_TESTING)
    add_test_executable(guest_harfbuzz_contextual_gsub_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzContextualGsub.cpp)
    target_include_directories(guest_harfbuzz_contextual_gsub_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_contextual_gsub_tests PRIVATE harfbuzz_backend ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_harfbuzz_contextual_gsub_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_harfbuzz_contextual_gsub_tests)
    set(harfbuzzContextualGsubModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_contextual_gsub_modules)
    set(harfbuzzContextualGsubModule ${harfbuzzContextualGsubModuleDir}/libharfbuzz.prx)
    set(harfbuzzContextualGsubKernel ${harfbuzzContextualGsubModuleDir}/libkernel.prx)
    set(harfbuzzContextualGsubLibc ${harfbuzzContextualGsubModuleDir}/libc.prx)
    set(harfbuzzContextualGsubExecutable ${harfbuzzContextualGsubModuleDir}/guest_harfbuzz_contextual_gsub_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(
        OUTPUT ${harfbuzzContextualGsubModule} ${harfbuzzContextualGsubKernel} ${harfbuzzContextualGsubLibc}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzContextualGsubModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzContextualGsubModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzContextualGsubKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzContextualGsubLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzContextualGsubModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzContextualGsubKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzContextualGsubLibc}
        DEPENDS nid_patcher libharfbuzz libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_harfbuzz_contextual_gsub_libraries DEPENDS ${harfbuzzContextualGsubModule} ${harfbuzzContextualGsubKernel} ${harfbuzzContextualGsubLibc})
    add_dependencies(guest_harfbuzz_contextual_gsub_tests guest_harfbuzz_contextual_gsub_libraries)
    add_custom_command(TARGET guest_harfbuzz_contextual_gsub_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_contextual_gsub_tests> ${harfbuzzContextualGsubExecutable}
        COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzContextualGsubExecutable}
        BYPRODUCTS ${harfbuzzContextualGsubExecutable}
        VERBATIM)
    add_test(NAME guest_harfbuzz_contextual_gsub COMMAND guest_harfbuzz_contextual_gsub_tests $<TARGET_FILE:libharfbuzz> unpatched)
    add_test(NAME guest_harfbuzz_contextual_gsub_nid COMMAND ${harfbuzzContextualGsubExecutable} ${harfbuzzContextualGsubModule} patched)
    set_tests_properties(guest_harfbuzz_contextual_gsub guest_harfbuzz_contextual_gsub_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz_contextual_gsub PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        set_tests_properties(guest_harfbuzz_contextual_gsub_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_contextual_gsub_tests>;PATH=path_list_prepend:${harfbuzzContextualGsubModuleDir}")
    elseif(APPLE)
        set_tests_properties(guest_harfbuzz_contextual_gsub PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_contextual_gsub_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_contextual_gsub_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzContextualGsubModuleDir}")
    else()
        set_tests_properties(guest_harfbuzz_contextual_gsub PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_contextual_gsub_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_contextual_gsub_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzContextualGsubModuleDir}")
    endif()
endif()
