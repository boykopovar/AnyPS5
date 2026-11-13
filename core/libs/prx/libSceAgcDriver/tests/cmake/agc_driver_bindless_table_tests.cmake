set(agcBindlessLookupSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/BindlessLookup.comp)
set(agcBindlessLookupSpv ${agcTestShaderGeneratedRoot}/BindlessLookup.spv)
set(agcBindlessLookupHeader ${agcTestShaderGeneratedRoot}/BindlessLookup_spv.h)
add_custom_command(
        OUTPUT "${agcBindlessLookupHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.2 -o "${agcBindlessLookupSpv}" "${agcBindlessLookupSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcBindlessLookupSpv} -DOUTPUT=${agcBindlessLookupHeader}
        -DSYMBOL=BINDLESS_LOOKUP_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcBindlessLookupSource}" glslang-standalone
        VERBATIM
)

add_test_executable(agc_driver_bindless_table_tests EXCLUDE_FROM_ALL tests/BindlessTable.cpp ${agcBindlessLookupHeader})
target_include_directories(agc_driver_bindless_table_tests PRIVATE ${LIBS_INCLUDE_DIR} ${agcTestShaderGeneratedRoot} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_driver_bindless_table_tests PRIVATE libSceAgcDriver SDL2-static libc)
set_target_properties(agc_driver_bindless_table_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_bindless_table_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_bindless_table_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_bindless_table COMMAND agc_driver_bindless_table_tests)
    add_test(NAME agc_driver_bindless_flag_off COMMAND agc_driver_bindless_table_tests --flag-off)
    add_test(NAME agc_driver_bindless_flag_on COMMAND agc_driver_bindless_table_tests --flag-on)
    set_tests_properties(agc_driver_bindless_table agc_driver_bindless_flag_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_driver_bindless_flag_on PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_BINDLESS_IMAGES=1" SKIP_RETURN_CODE 77 TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_bindless_table agc_driver_bindless_flag_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>;APS5_BINDLESS_IMAGES=unset:")
        set_tests_properties(agc_driver_bindless_flag_on PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    else()
        set_tests_properties(agc_driver_bindless_table agc_driver_bindless_flag_off PROPERTIES ENVIRONMENT_MODIFICATION "APS5_BINDLESS_IMAGES=unset:")
    endif()
endif()
