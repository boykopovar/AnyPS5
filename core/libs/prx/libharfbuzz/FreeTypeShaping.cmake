add_test_executable(guest_harfbuzz_freetype_shaping_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzFreeTypeShaping.cpp)
target_include_directories(guest_harfbuzz_freetype_shaping_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(guest_harfbuzz_freetype_shaping_tests PRIVATE harfbuzz_backend freetype ${CMAKE_DL_LIBS})
target_compile_definitions(guest_harfbuzz_freetype_shaping_tests PRIVATE HARFBUZZ_TEST_FONTS="${CMAKE_SOURCE_DIR}/3rdparty/harfbuzz/test/shape/data/text-rendering-tests/fonts")
if(WIN32)
    target_link_libraries(guest_harfbuzz_freetype_shaping_tests PRIVATE libc)
endif()
configure_windows_unwind(guest_harfbuzz_freetype_shaping_tests)
set(harfbuzzFreeTypeShapingModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_freetype_shaping_modules)
set(harfbuzzFreeTypeShapingModule ${harfbuzzFreeTypeShapingModuleDir}/libharfbuzz.prx)
set(harfbuzzFreeTypeShapingFreeType ${harfbuzzFreeTypeShapingModuleDir}/libSceFreeType.prx)
set(harfbuzzFreeTypeShapingKernel ${harfbuzzFreeTypeShapingModuleDir}/libkernel.prx)
set(harfbuzzFreeTypeShapingLibc ${harfbuzzFreeTypeShapingModuleDir}/libc.prx)
set(harfbuzzFreeTypeShapingExecutable ${harfbuzzFreeTypeShapingModuleDir}/guest_harfbuzz_freetype_shaping_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
add_custom_command(
    OUTPUT ${harfbuzzFreeTypeShapingModule} ${harfbuzzFreeTypeShapingFreeType} ${harfbuzzFreeTypeShapingKernel} ${harfbuzzFreeTypeShapingLibc}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzFreeTypeShapingModuleDir}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzFreeTypeShapingModule}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libSceFreeType> ${harfbuzzFreeTypeShapingFreeType}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzFreeTypeShapingKernel}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzFreeTypeShapingLibc}
    COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzFreeTypeShapingModule}
    COMMAND $<TARGET_FILE:nid_patcher> libSceFreeType --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzFreeTypeShapingFreeType}
    COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzFreeTypeShapingKernel}
    COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzFreeTypeShapingLibc}
    DEPENDS nid_patcher libharfbuzz libSceFreeType libkernel libc ${LIBC_EXPORT_LIST}
    VERBATIM)
add_custom_target(guest_harfbuzz_freetype_shaping_libraries DEPENDS ${harfbuzzFreeTypeShapingModule} ${harfbuzzFreeTypeShapingFreeType} ${harfbuzzFreeTypeShapingKernel} ${harfbuzzFreeTypeShapingLibc})
add_dependencies(guest_harfbuzz_freetype_shaping_tests guest_harfbuzz_freetype_shaping_libraries)
add_custom_command(TARGET guest_harfbuzz_freetype_shaping_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_freetype_shaping_tests> ${harfbuzzFreeTypeShapingExecutable}
    COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzFreeTypeShapingExecutable}
    BYPRODUCTS ${harfbuzzFreeTypeShapingExecutable}
    VERBATIM)
add_test(NAME guest_harfbuzz_freetype_shaping COMMAND guest_harfbuzz_freetype_shaping_tests $<TARGET_FILE:libharfbuzz> $<TARGET_FILE:libSceFreeType> unpatched)
add_test(NAME guest_harfbuzz_freetype_shaping_nid COMMAND ${harfbuzzFreeTypeShapingExecutable} ${harfbuzzFreeTypeShapingModule} ${harfbuzzFreeTypeShapingFreeType} patched)
set_tests_properties(guest_harfbuzz_freetype_shaping guest_harfbuzz_freetype_shaping_nid PROPERTIES TIMEOUT 20)
if(WIN32)
    set_tests_properties(guest_harfbuzz_freetype_shaping PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
    set_tests_properties(guest_harfbuzz_freetype_shaping_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_freetype_shaping_tests>;PATH=path_list_prepend:${harfbuzzFreeTypeShapingModuleDir}")
elseif(APPLE)
    set_tests_properties(guest_harfbuzz_freetype_shaping PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
    set_tests_properties(guest_harfbuzz_freetype_shaping_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_freetype_shaping_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzFreeTypeShapingModuleDir}")
else()
    set_tests_properties(guest_harfbuzz_freetype_shaping PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
    set_tests_properties(guest_harfbuzz_freetype_shaping_nid PROPERTIES ENVIRONMENT_MODIFICATION
        "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_freetype_shaping_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzFreeTypeShapingModuleDir}")
endif()
