set(harfbuzzSourceDir ${CMAKE_SOURCE_DIR}/3rdparty/harfbuzz)
add_library(harfbuzz_backend STATIC EXCLUDE_FROM_ALL ${harfbuzzSourceDir}/src/harfbuzz.cc)
target_include_directories(harfbuzz_backend PUBLIC ${harfbuzzSourceDir}/src)
set_target_properties(harfbuzz_backend PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
install(FILES ${harfbuzzSourceDir}/COPYING DESTINATION licenses/harfbuzz)
install(FILES ${harfbuzzSourceDir}/src/ms-use/COPYING DESTINATION licenses/harfbuzz/ms-use)

add_library(libharfbuzz SHARED EXCLUDE_FROM_ALL Export.cpp Buffer.cpp FontFunctions.cpp Layout.cpp)
target_include_directories(libharfbuzz PUBLIC ${LIBS_INCLUDE_DIR})
target_link_libraries(libharfbuzz PRIVATE harfbuzz_backend)
target_sources(libharfbuzz PRIVATE Lifecycle.cpp)
configure_windows_unwind(libharfbuzz)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libharfbuzz PRIVATE LINKER:--exclude-libs,ALL)
endif()

include(Layout.cmake)

if(BUILD_TESTING)
    add_test_executable(guest_harfbuzz_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzz.cpp)
    target_include_directories(guest_harfbuzz_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_tests PRIVATE libharfbuzz libc)
    configure_windows_unwind(guest_harfbuzz_tests)
    add_test(NAME guest_harfbuzz COMMAND guest_harfbuzz_tests)
    set_tests_properties(guest_harfbuzz PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
    endif()
    add_test_executable(guest_harfbuzz_buffer_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzBuffer.cpp)
    target_link_libraries(guest_harfbuzz_buffer_tests PRIVATE libharfbuzz harfbuzz_backend libc)
    configure_windows_unwind(guest_harfbuzz_buffer_tests)
    foreach(mode IN ITEMS utf16 properties lifetime)
        add_test(NAME guest_harfbuzz_buffer_${mode} COMMAND guest_harfbuzz_buffer_tests ${mode})
        set_tests_properties(guest_harfbuzz_buffer_${mode} PROPERTIES TIMEOUT 20 ENVIRONMENT "LC_ALL=C;LANG=C")
        if(WIN32)
            set_tests_properties(guest_harfbuzz_buffer_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        endif()
    endforeach()
    include(Lifecycle.cmake)
    add_test_executable(guest_harfbuzz_font_tests ${LIBS_INCLUDE_DIR}/tests/GuestHarfBuzzFont.cpp)
    target_include_directories(guest_harfbuzz_font_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_harfbuzz_font_tests PRIVATE harfbuzz_backend ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_harfbuzz_font_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_harfbuzz_font_tests)
    set(harfbuzzFontModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_harfbuzz_font_modules)
    set(harfbuzzFontModule ${harfbuzzFontModuleDir}/libharfbuzz.prx)
    set(harfbuzzFontKernel ${harfbuzzFontModuleDir}/libkernel.prx)
    set(harfbuzzFontLibc ${harfbuzzFontModuleDir}/libc.prx)
    set(harfbuzzFontExecutable ${harfbuzzFontModuleDir}/guest_harfbuzz_font_nid_tests${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(
        OUTPUT ${harfbuzzFontModule} ${harfbuzzFontKernel} ${harfbuzzFontLibc}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${harfbuzzFontModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libharfbuzz> ${harfbuzzFontModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${harfbuzzFontKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${harfbuzzFontLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libharfbuzz --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzFontModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${harfbuzzFontKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${harfbuzzFontLibc}
        DEPENDS nid_patcher libharfbuzz libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_harfbuzz_font_libraries DEPENDS ${harfbuzzFontModule} ${harfbuzzFontKernel} ${harfbuzzFontLibc})
    add_dependencies(guest_harfbuzz_font_tests guest_harfbuzz_font_libraries)
    add_custom_command(TARGET guest_harfbuzz_font_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:guest_harfbuzz_font_tests> ${harfbuzzFontExecutable}
        COMMAND $<TARGET_FILE:nid_patcher> --exec --only-sce ${harfbuzzFontExecutable}
        BYPRODUCTS ${harfbuzzFontExecutable}
        VERBATIM)
    add_test(NAME guest_harfbuzz_font COMMAND guest_harfbuzz_font_tests $<TARGET_FILE:libharfbuzz> unpatched)
    add_test(NAME guest_harfbuzz_font_nid COMMAND ${harfbuzzFontExecutable} ${harfbuzzFontModule} patched)
    set_tests_properties(guest_harfbuzz_font guest_harfbuzz_font_nid PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_harfbuzz_font PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libharfbuzz>")
        set_tests_properties(guest_harfbuzz_font_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_font_tests>;PATH=path_list_prepend:${harfbuzzFontModuleDir}")
    elseif(APPLE)
        set_tests_properties(guest_harfbuzz_font PROPERTIES ENVIRONMENT_MODIFICATION "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_font_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_font_tests>;DYLD_LIBRARY_PATH=path_list_prepend:${harfbuzzFontModuleDir}")
    else()
        set_tests_properties(guest_harfbuzz_font PROPERTIES ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR}")
        set_tests_properties(guest_harfbuzz_font_nid PROPERTIES ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:guest_harfbuzz_font_tests>;LD_LIBRARY_PATH=path_list_prepend:${harfbuzzFontModuleDir}")
    endif()
endif()
