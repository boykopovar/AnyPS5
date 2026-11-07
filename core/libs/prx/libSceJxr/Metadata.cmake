add_library(jxr_metadata_native STATIC
    ${JXR_SOURCE_DIR}/jxrgluelib/JXRGlueJxr.c
    src/NativeMetadata.c)
configure_jxr_native(jxr_metadata_native)
set_source_files_properties(${JXR_SOURCE_DIR}/jxrgluelib/JXRGlueJxr.c TARGET_DIRECTORY jxr_metadata_native
    PROPERTIES COMPILE_DEFINITIONS "PKFree=JxrMetadataFree_nid_no_patch;FreeDescMetadata=JxrMetadataRelease_nid_no_patch")
target_link_libraries(jxr_metadata_native PRIVATE jxr)
target_sources(libSceJxr PRIVATE src/Metadata.cpp)
target_link_libraries(libSceJxr PRIVATE jxr_metadata_native)

if(BUILD_TESTING)
    if(MINGW)
        add_executable(guest_jxr_metadata_tests tests/Metadata.cpp $<TARGET_OBJECTS:anyps5_windows_test_exit>)
        target_link_options(guest_jxr_metadata_tests PRIVATE LINKER:--wrap=main)
        set_target_properties(guest_jxr_metadata_tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}/jxr-metadata>")
        get_filename_component(metadataCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(runtime IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            add_custom_command(TARGET guest_jxr_metadata_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${metadataCompilerDirectory}/${runtime}"
                    "$<TARGET_FILE_DIR:guest_jxr_metadata_tests>" VERBATIM)
        endforeach()
    else()
        add_test_executable(guest_jxr_metadata_tests tests/Metadata.cpp)
    endif()
    target_include_directories(guest_jxr_metadata_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jxr_metadata_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_jxr_metadata_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_jxr_metadata_tests)
    add_custom_target(jxr_metadata_patched DEPENDS ${PATCHED_DIR}/libSceJxr.prx ${PATCHED_DIR}/libkernel.prx ${LIBC_PATCHED})
    add_dependencies(guest_jxr_metadata_tests jxr_metadata_patched)
    add_test(NAME guest_jxr_metadata COMMAND guest_jxr_metadata_tests
        $<TARGET_FILE:libc> $<TARGET_FILE:libSceJxr> FreeDescMetadata_nid_postfix)
    add_test(NAME guest_jxr_metadata_patched COMMAND guest_jxr_metadata_tests
        ${LIBC_PATCHED} ${PATCHED_DIR}/libSceJxr.prx "XmDuKlVaRuo")
    set_tests_properties(guest_jxr_metadata guest_jxr_metadata_patched PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_jxr_metadata PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJxr>")
        set_tests_properties(guest_jxr_metadata_patched PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${PATCHED_DIR}")
    endif()
endif()
