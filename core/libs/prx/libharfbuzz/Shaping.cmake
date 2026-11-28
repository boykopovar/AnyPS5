target_sources(libharfbuzz PRIVATE Shaping.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_harfbuzz_shape_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzShaping.cpp)
    target_include_directories(guest_harfbuzz_shape_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_shape_tests PRIVATE harfbuzz_backend ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_harfbuzz_shape_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_harfbuzz_shape_tests)
    set(harfbuzzShapingModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_shape_modules)
    set(harfbuzzShapingModule ${harfbuzzShapingModuleDir}/libharfbuzz.prx)
    set(harfbuzzShapingKernel ${harfbuzzShapingModuleDir}/libkernel.prx)
    set(harfbuzzShapingLibc ${harfbuzzShapingModuleDir}/libc.prx)
    set(harfbuzzShapingExecutable ${harfbuzzShapingModuleDir}/guest_harfbuzz_shape_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(
        OUTPUT ${harfbuzzShapingModule} ${harfbuzzShapingKernel} ${harfbuzzShapingLibc}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzShapingModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzShapingModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzShapingKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzShapingLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzShapingModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzShapingKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzShapingLibc}
        DEPENDS nid_patcher libharfbuzz libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_harfbuzz_shape_libraries DEPENDS ${harfbuzzShapingModule} ${harfbuzzShapingKernel} ${harfbuzzShapingLibc})
    add_dependencies(guest_harfbuzz_shape_tests guest_harfbuzz_shape_libraries)
    add_custom_command(TARGET guest_harfbuzz_shape_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_shape_tests> ${harfbuzzShapingExecutable}
        COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzShapingExecutable}
        BYPRODUCTS ${harfbuzzShapingExecutable}
        VERBATIM)
    add_test(NAME guest_harfbuzz_shape COMMAND guest_harfbuzz_shape_tests $<TARGET_FILE:libharfbuzz> unpatched)
    add_test(NAME guest_harfbuzz_shape_nid COMMAND ${harfbuzzShapingExecutable} ${harfbuzzShapingModule} patched)
    set_tests_properties(guest_harfbuzz_shape guest_harfbuzz_shape_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz_shape PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        set_tests_properties(guest_harfbuzz_shape_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_shape_tests>;PATH=path_list_prepend:${harfbuzzShapingModuleDir}")
    elseif(APPLE)
        set_tests_properties(guest_harfbuzz_shape PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_shape_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_shape_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzShapingModuleDir}")
    else()
        set_tests_properties(guest_harfbuzz_shape PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_shape_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_shape_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzShapingModuleDir}")
    endif()
endif()
