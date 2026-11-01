set(harfbuzzSourceDir ${CMAKE_SOURCE_DIR}/3rdparty/harfbuzz)
add_library(harfbuzz_backend STATIC EXCLUDE_FROM_ALL ${harfbuzzSourceDir}/src/harfbuzz.cc)
target_include_directories(harfbuzz_backend PUBLIC ${harfbuzzSourceDir}/src)
set_target_properties(harfbuzz_backend PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
install(FILES ${harfbuzzSourceDir}/COPYING DESTINATION licenses/harfbuzz)

add_library(libharfbuzz SHARED EXCLUDE_FROM_ALL Export.cpp Buffer.cpp)
target_include_directories(libharfbuzz PUBLIC ${LIBS_INCLUDE_DIR})
target_link_libraries(libharfbuzz PRIVATE harfbuzz_backend)
configure_windows_unwind(libharfbuzz)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libharfbuzz PRIVATE LINKER:--exclude-libs,ALL)
endif()

if(BUILD_TESTING)
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
endif()
