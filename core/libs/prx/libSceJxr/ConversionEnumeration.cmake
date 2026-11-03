add_library(jxr_conversion_enumeration_native STATIC src/NativeConversionEnumeration.c)
configure_jxr_native(jxr_conversion_enumeration_native)
target_link_libraries(jxr_conversion_enumeration_native PRIVATE jxr)
target_sources(libSceJxr PRIVATE src/ConversionEnumeration.cpp)
target_link_libraries(libSceJxr PRIVATE jxr_conversion_enumeration_native)

if(BUILD_TESTING)
    add_library(jxr_conversion_enumeration_reference STATIC tests/ConversionReference.c)
    configure_jxr_native(jxr_conversion_enumeration_reference)
    target_link_libraries(jxr_conversion_enumeration_reference PRIVATE jxr)
    if(MINGW)
        add_executable(guest_jxr_conversion_enumeration_tests tests/ConversionEnumeration.cpp $<TARGET_OBJECTS:anyps5_windows_test_exit>)
        target_link_options(guest_jxr_conversion_enumeration_tests PRIVATE LINKER:--wrap=main)
        set_target_properties(guest_jxr_conversion_enumeration_tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<1:${ANYPS5_TEST_OUTPUT_DIRECTORY}/jxr-conversions>")
        get_filename_component(conversionCompilerDirectory "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(runtime IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            add_custom_command(TARGET guest_jxr_conversion_enumeration_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${conversionCompilerDirectory}/${runtime}"
                    "$<TARGET_FILE_DIR:guest_jxr_conversion_enumeration_tests>" VERBATIM)
        endforeach()
    else()
        add_test_executable(guest_jxr_conversion_enumeration_tests tests/ConversionEnumeration.cpp)
    endif()
    target_include_directories(guest_jxr_conversion_enumeration_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_jxr_conversion_enumeration_tests PRIVATE jxr_conversion_enumeration_reference ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_jxr_conversion_enumeration_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_jxr_conversion_enumeration_tests)
    add_custom_target(jxr_conversion_enumeration_patched DEPENDS ${PATCHED_DIR}/libSceJxr.prx ${PATCHED_DIR}/libkernel.prx ${LIBC_PATCHED})
    add_dependencies(guest_jxr_conversion_enumeration_tests jxr_conversion_enumeration_patched)
    add_test(NAME guest_jxr_conversion_enumeration COMMAND guest_jxr_conversion_enumeration_tests
        $<TARGET_FILE:libSceJxr> PKFormatConverter_EnumConversions_nid_postfix)
    add_test(NAME guest_jxr_conversion_enumeration_patched COMMAND guest_jxr_conversion_enumeration_tests
        ${PATCHED_DIR}/libSceJxr.prx "g5p4acBy56A")
    set_tests_properties(guest_jxr_conversion_enumeration guest_jxr_conversion_enumeration_patched PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_jxr_conversion_enumeration PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceJxr>")
        set_tests_properties(guest_jxr_conversion_enumeration_patched PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${PATCHED_DIR}")
    endif()
endif()
