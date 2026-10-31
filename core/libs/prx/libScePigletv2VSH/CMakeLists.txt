add_library(libScePigletv2VSH SHARED EXCLUDE_FROM_ALL
        Export.cpp
        src/Context.cpp
        src/Egl.cpp
        src/Program.cpp
        src/Vulkan.cpp
)
target_include_directories(libScePigletv2VSH PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(libScePigletv2VSH PRIVATE libSceVideoOut libSceAgcDriver libc)
if(MINGW)
    target_link_libraries(libScePigletv2VSH PRIVATE $<TARGET_LINKER_FILE:libc> glslang SPIRV glslang-default-resource-limits SDL2-static stdc++)
else()
    target_link_libraries(libScePigletv2VSH PRIVATE glslang SPIRV glslang-default-resource-limits SDL2-static)
endif()
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libScePigletv2VSH PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libScePigletv2VSH)

if(BUILD_TESTING)
    add_test_executable(guest_piglet_tests ${LIBS_INCLUDE_DIR}/tests/GuestPiglet.cpp)
    target_include_directories(guest_piglet_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(guest_piglet_tests PRIVATE libScePigletv2VSH libScePrecompiledShaders libc)
    configure_windows_unwind(guest_piglet_tests)
    add_test(NAME guest_piglet COMMAND guest_piglet_tests)
    set_tests_properties(guest_piglet PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(guest_piglet PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePigletv2VSH>")
    endif()
endif()
