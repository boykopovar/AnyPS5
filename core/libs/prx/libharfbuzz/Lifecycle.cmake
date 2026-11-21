add_test_executable(guest_harfbuzz_lifecycle_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzLifecycle.cpp)
target_include_directories(guest_harfbuzz_lifecycle_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(guest_harfbuzz_lifecycle_tests PRIVATE harfbuzz_backend freetype ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(guest_harfbuzz_lifecycle_tests PRIVATE libc)
endif()
configure_windows_unwind(guest_harfbuzz_lifecycle_tests)
set(harfbuzzLifecycleModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_lifecycle_modules)
set(harfbuzzLifecycleModule ${harfbuzzLifecycleModuleDir}/libharfbuzz.prx)
set(harfbuzzLifecycleFreeType ${harfbuzzLifecycleModuleDir}/libSceFreeType.prx)
set(harfbuzzLifecycleKernel ${harfbuzzLifecycleModuleDir}/libkernel.prx)
set(harfbuzzLifecycleLibc ${harfbuzzLifecycleModuleDir}/libc.prx)
set(harfbuzzLifecycleExecutable ${harfbuzzLifecycleModuleDir}/guest_harfbuzz_lifecycle_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
add_custom_command(
    OUTPUT ${harfbuzzLifecycleModule} ${harfbuzzLifecycleFreeType} ${harfbuzzLifecycleKernel} ${harfbuzzLifecycleLibc}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzLifecycleModuleDir}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzLifecycleModule}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libSceFreeType> ${harfbuzzLifecycleFreeType}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzLifecycleKernel}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzLifecycleLibc}
    COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzLifecycleModule}
    COMMAND $<TARGET_FILE:nid_patcher> libSceFreeType --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzLifecycleFreeType}
    COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzLifecycleKernel}
    COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzLifecycleLibc}
    DEPENDS nid_patcher libharfbuzz libSceFreeType libkernel libc ${LIBC_EXPORT_LIST}
    VERBATIM)
add_custom_target(guest_harfbuzz_lifecycle_libraries DEPENDS ${harfbuzzLifecycleModule} ${harfbuzzLifecycleFreeType} ${harfbuzzLifecycleKernel} ${harfbuzzLifecycleLibc})
add_dependencies(guest_harfbuzz_lifecycle_tests guest_harfbuzz_lifecycle_libraries)
add_custom_command(TARGET guest_harfbuzz_lifecycle_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_lifecycle_tests> ${harfbuzzLifecycleExecutable}
    COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzLifecycleExecutable}
    BYPRODUCTS ${harfbuzzLifecycleExecutable}
    VERBATIM)
add_test(NAME guest_harfbuzz_lifecycle COMMAND guest_harfbuzz_lifecycle_tests $<TARGET_FILE:libharfbuzz> $<TARGET_FILE:libSceFreeType> unpatched)
add_test(NAME guest_harfbuzz_lifecycle_nid COMMAND ${harfbuzzLifecycleExecutable} ${harfbuzzLifecycleModule} ${harfbuzzLifecycleFreeType} patched)
set_tests_properties(guest_harfbuzz_lifecycle guest_harfbuzz_lifecycle_nid PROPERTIES TIMEOUT 20)
if(WIN32)
    set_tests_properties(guest_harfbuzz_lifecycle PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
    set_tests_properties(guest_harfbuzz_lifecycle_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_lifecycle_tests>;PATH=path_list_prepend:${harfbuzzLifecycleModuleDir}")
elseif(APPLE)
    set_tests_properties(guest_harfbuzz_lifecycle PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
    set_tests_properties(guest_harfbuzz_lifecycle_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_lifecycle_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzLifecycleModuleDir}")
else()
    set_tests_properties(guest_harfbuzz_lifecycle PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
    set_tests_properties(guest_harfbuzz_lifecycle_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_lifecycle_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzLifecycleModuleDir}")
endif()
