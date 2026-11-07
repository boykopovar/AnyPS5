foreach(agcReadbackTestKind IN ITEMS Vertex Uint Depth)
    string(TOUPPER ${agcReadbackTestKind} agcReadbackTestSymbol)
    if(agcReadbackTestKind STREQUAL "Vertex")
        set(agcReadbackTestStage vert)
    else()
        set(agcReadbackTestStage frag)
    endif()
    if(agcReadbackTestKind STREQUAL "Depth")
        set(agcReadbackTestDepth 1)
    else()
        set(agcReadbackTestDepth 0)
    endif()
    set(agcReadbackTestHeader ${agcTestShaderGeneratedRoot}/MsaaReadback${agcReadbackTestKind}_spv.h)
    set(agcReadbackTestSpv ${agcTestShaderGeneratedRoot}/MsaaReadback${agcReadbackTestKind}.spv)
    add_custom_command(
            OUTPUT "${agcReadbackTestHeader}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
            COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -DREAD_DEPTH=${agcReadbackTestDepth} -o "${agcReadbackTestSpv}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/MultisampleReadback.${agcReadbackTestStage}"
            COMMAND ${CMAKE_COMMAND} -DINPUT=${agcReadbackTestSpv} -DOUTPUT=${agcReadbackTestHeader}
            -DSYMBOL=MSAA_READBACK_${agcReadbackTestSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/MultisampleReadback.${agcReadbackTestStage}" glslang-standalone
            VERBATIM
    )
    target_sources(agc_driver_multisample_readback_tests PRIVATE ${agcReadbackTestHeader})
endforeach()
target_include_directories(agc_driver_multisample_readback_tests PRIVATE ${agcTestShaderGeneratedRoot})

if(BUILD_TESTING)
    set_tests_properties(agc_driver_multisample_readback PROPERTIES TIMEOUT 20)
    foreach(agcReadbackCase IN ITEMS uint8 uint16 uint64 uint128 depth upload retile depth_retile)
        add_test(NAME agc_driver_multisample_readback_${agcReadbackCase} COMMAND agc_driver_multisample_readback_tests ${agcReadbackCase})
        set_tests_properties(agc_driver_multisample_readback_${agcReadbackCase} PROPERTIES TIMEOUT 20 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_multisample_readback_${agcReadbackCase} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
