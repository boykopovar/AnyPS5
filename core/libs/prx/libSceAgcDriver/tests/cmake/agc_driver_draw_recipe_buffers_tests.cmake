set(agcDrawRecipeBuffersSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/DrawRecipeBuffers.frag)
set(agcDrawRecipeHeaders)
foreach(agcDrawRecipeVariant IN ITEMS Buffers Data)
    set(agcDrawRecipeSpv ${agcTestShaderGeneratedRoot}/DrawRecipe${agcDrawRecipeVariant}.spv)
    set(agcDrawRecipeHeader ${agcTestShaderGeneratedRoot}/DrawRecipe${agcDrawRecipeVariant}_spv.h)
    string(TOUPPER "${agcDrawRecipeVariant}" agcDrawRecipeSymbol)
    set(agcDrawRecipeDefines)
    if(agcDrawRecipeVariant STREQUAL "Data")
        set(agcDrawRecipeDefines -DDATA_WORDS=1)
    endif()
    add_custom_command(
        OUTPUT "${agcDrawRecipeHeader}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
        COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 ${agcDrawRecipeDefines} -o "${agcDrawRecipeSpv}" "${agcDrawRecipeBuffersSource}"
        COMMAND ${CMAKE_COMMAND} -DINPUT=${agcDrawRecipeSpv} -DOUTPUT=${agcDrawRecipeHeader}
        -DSYMBOL=DRAW_RECIPE_${agcDrawRecipeSymbol}_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
        DEPENDS "${agcDrawRecipeBuffersSource}" glslang-standalone
        VERBATIM
    )
    list(APPEND agcDrawRecipeHeaders ${agcDrawRecipeHeader})
endforeach()
target_sources(agc_driver_draw_recipe_buffers_tests PRIVATE ${agcCoverShaderHeaders} ${agcDrawRecipeHeaders})
target_include_directories(agc_driver_draw_recipe_buffers_tests PRIVATE ${agcTestShaderGeneratedRoot})

if(BUILD_TESTING)
    add_test(NAME agc_driver_draw_recipe_data COMMAND agc_driver_draw_recipe_buffers_tests --data)
    set_tests_properties(agc_driver_draw_recipe_buffers agc_driver_draw_recipe_data PROPERTIES
            SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_draw_recipe_data PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
