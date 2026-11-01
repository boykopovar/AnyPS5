foreach(agcSampleOrderTestVariant IN ITEMS "R8;SampleOrderTexels.comp;-DTEXEL_FORMAT=r8ui" "R16;SampleOrderTexels.comp;-DTEXEL_FORMAT=r16ui" "R32;SampleOrderTexels.comp;-DTEXEL_FORMAT=r32ui" "RG32;SampleOrderTexels.comp;-DTEXEL_FORMAT=rg32ui" "RGBA32;SampleOrderTexels.comp;-DTEXEL_FORMAT=rgba32ui" "FillVertex;SampleOrderFill.vert;-DPLAIN=1" "FillDepth;SampleOrderFill.frag;-DPLAIN=1" "FillStencil;SampleOrderFill.frag;-DSTENCIL=1" "ReadDepth;SampleOrderRead.comp;-DPLAIN=1" "ReadStencil;SampleOrderRead.comp;-DSTENCIL=1" "ReadColor;SampleOrderRead.comp;-DCOLOR=1")
    list(GET agcSampleOrderTestVariant 0 agcSampleOrderTestName)
    list(GET agcSampleOrderTestVariant 1 agcSampleOrderTestFile)
    list(GET agcSampleOrderTestVariant 2 agcSampleOrderTestDefine)
    string(TOUPPER "${agcSampleOrderTestName}" agcSampleOrderTestSymbol)
    set(agcSampleOrderTestSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/${agcSampleOrderTestFile})
    set(agcSampleOrderTestSpv ${agcTestShaderGeneratedRoot}/SampleOrderTest${agcSampleOrderTestName}.spv)
    set(agcSampleOrderTestHeader ${agcTestShaderGeneratedRoot}/SampleOrderTest${agcSampleOrderTestName}_spv.h)
    add_custom_command(
        OUTPUT "${agcSampleOrderTestHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 ${agcSampleOrderTestDefine} -o "${agcSampleOrderTestSpv}" "${agcSampleOrderTestSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcSampleOrderTestSpv} -DOUTPUT=${agcSampleOrderTestHeader}
        -DSYMBOL=SAMPLE_ORDER_TEST_${agcSampleOrderTestSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcSampleOrderTestSource}" glslang-standalone
        VERBATIM
    )
    target_sources(agc_driver_sample_order_tests PRIVATE ${agcSampleOrderTestHeader})
endforeach()
target_include_directories(agc_driver_sample_order_tests PRIVATE ${agcTestShaderGeneratedRoot})

if(BUILD_TESTING)
    set_tests_properties(agc_driver_sample_order PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sample_order_grid_reference COMMAND agc_driver_sample_order_tests --grid-reference)
    add_test(NAME agc_driver_sample_order_recipe_depth_locations COMMAND agc_driver_sample_order_tests --recipe-depth-locations)
    set_tests_properties(agc_driver_sample_order_grid_reference agc_driver_sample_order_recipe_depth_locations PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_sample_order_grid_reference agc_driver_sample_order_recipe_depth_locations PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
