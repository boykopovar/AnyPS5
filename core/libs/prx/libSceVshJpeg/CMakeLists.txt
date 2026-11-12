add_library(libSceVshJpeg SHARED EXCLUDE_FROM_ALL Export.cpp Compress.cpp)
target_include_directories(libSceVshJpeg PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src
    ${CMAKE_BINARY_DIR}/libjpeg-turbo/include)
set_target_properties(libSceVshJpeg PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceVshJpeg)
target_link_libraries(libSceVshJpeg PRIVATE jpeg_turbo libSceNKWebKitRequirements)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceVshJpeg PRIVATE LINKER:--exclude-libs,ALL)
endif()

if(BUILD_TESTING)
    add_test_executable(guest_vsh_jpeg_compress_tests tests/Compress.cpp)
    target_include_directories(guest_vsh_jpeg_compress_tests PRIVATE ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src
        ${CMAKE_BINARY_DIR}/libjpeg-turbo/include)
    target_link_libraries(guest_vsh_jpeg_compress_tests PRIVATE libSceVshJpeg libSceNKWebKitRequirements jpeg_turbo libc)
    configure_windows_unwind(guest_vsh_jpeg_compress_tests)
    add_test(NAME guest_vsh_jpeg_compress COMMAND guest_vsh_jpeg_compress_tests)
    set_tests_properties(guest_vsh_jpeg_compress PROPERTIES TIMEOUT 30)
    if(NOT WIN32)
        add_executable(guest_vsh_jpeg_import_resolution_tests tests/ImportResolution.cpp)
        target_compile_definitions(guest_vsh_jpeg_import_resolution_tests PRIVATE VSH_JPEG_MODULE="$<TARGET_FILE:libSceVshJpeg>")
        target_link_libraries(guest_vsh_jpeg_import_resolution_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(guest_vsh_jpeg_import_resolution_tests libSceVshJpeg)
        add_test(NAME guest_vsh_jpeg_import_resolution COMMAND guest_vsh_jpeg_import_resolution_tests)
        set_tests_properties(guest_vsh_jpeg_import_resolution PROPERTIES TIMEOUT 30)
    endif()
    if(WIN32)
        set_tests_properties(guest_vsh_jpeg_compress PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVshJpeg>")
    endif()
endif()
