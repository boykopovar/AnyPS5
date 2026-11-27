foreach(shader IN ITEMS Cover.vert PackedVertexProducer.frag)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/${shader})
    string(REPLACE "." "_" stem "${shader}")
    set(output ${agcTestShaderGeneratedRoot}/PackedFetch_${stem})
    string(TOUPPER "PACKED_FETCH_${stem}_SPV" symbol)
    add_custom_command(OUTPUT "${output}.h"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${output}.spv" "${source}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${output}.spv -DOUTPUT=${output}.h -DSYMBOL=${symbol} -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${source}" glslang-standalone VERBATIM)
    target_sources(agc_driver_packed_vertex_fetch_tests PRIVATE "${output}.h")
endforeach()
target_include_directories(agc_driver_packed_vertex_fetch_tests PRIVATE ${agcTestShaderGeneratedRoot})
