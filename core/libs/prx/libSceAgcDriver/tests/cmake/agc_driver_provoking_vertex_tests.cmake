foreach(stage IN ITEMS vert frag)
    set(agcProvokingShaderSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/ProvokingVertex.${stage})
    set(agcProvokingShaderSpv ${agcTestShaderGeneratedRoot}/ProvokingVertex_${stage}.spv)
    set(agcProvokingShaderHeader ${agcTestShaderGeneratedRoot}/ProvokingVertex_${stage}_spv.h)
    string(TOUPPER "PROVOKING_VERTEX_${stage}_SPV" agcProvokingShaderSymbol)
    add_custom_command(
        OUTPUT "${agcProvokingShaderHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcProvokingShaderSpv}" "${agcProvokingShaderSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcProvokingShaderSpv} -DOUTPUT=${agcProvokingShaderHeader}
        -DSYMBOL=${agcProvokingShaderSymbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcProvokingShaderSource}" glslang-standalone
        VERBATIM
    )
    target_sources(agc_driver_provoking_vertex_tests PRIVATE ${agcProvokingShaderHeader})
endforeach()
target_include_directories(agc_driver_provoking_vertex_tests PRIVATE ${agcTestShaderGeneratedRoot})

if(BUILD_TESTING)
    set_tests_properties(agc_driver_provoking_vertex PROPERTIES TIMEOUT 30)
endif()
