set(agcDrawProofsSource ${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/DrawProofs.frag)
set(agcDrawProofsSpv ${agcTestShaderGeneratedRoot}/DrawProofs.spv)
set(agcDrawProofsHeader ${agcTestShaderGeneratedRoot}/DrawProofs_spv.h)
add_custom_command(
    OUTPUT "${agcDrawProofsHeader}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${agcTestShaderGeneratedRoot}"
    COMMAND "$<TARGET_FILE:glslang-standalone>" -V --target-env vulkan1.1 -o "${agcDrawProofsSpv}" "${agcDrawProofsSource}"
    COMMAND ${CMAKE_COMMAND} -DINPUT=${agcDrawProofsSpv} -DOUTPUT=${agcDrawProofsHeader}
    -DSYMBOL=DRAW_PROOFS_SPV -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
    DEPENDS "${agcDrawProofsSource}" glslang-standalone
    VERBATIM
)
target_sources(agc_driver_draw_proofs_tests PRIVATE ${agcCoverShaderHeaders} ${agcDrawProofsHeader})
target_include_directories(agc_driver_draw_proofs_tests PRIVATE ${agcTestShaderGeneratedRoot})

if(BUILD_TESTING)
    add_test(NAME agc_driver_draw_proofs_verify COMMAND agc_driver_draw_proofs_tests --verify)
    add_test(NAME agc_driver_draw_proofs_off COMMAND agc_driver_draw_proofs_tests --off)
    set_tests_properties(agc_driver_draw_proofs_verify PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PROOFS=1")
    set_tests_properties(agc_driver_draw_proofs_off PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_PROOFS=1")
    set(agcDrawProofTests agc_driver_draw_proofs_verify agc_driver_draw_proofs_off)
    foreach(agcVerifiedDrawTest IN ITEMS draw_cache_reuse draw_cache_relocation draw_recapture_on_change draw_recipe_buffers draw_write_ranges pass_input_hazard recorded_draw_staging linear_target_resident resident_target_register_clear finisher_label_order pipelined_metadata_pass)
        add_test(NAME agc_driver_${agcVerifiedDrawTest}_verify_proofs COMMAND agc_driver_${agcVerifiedDrawTest}_tests)
        set_tests_properties(agc_driver_${agcVerifiedDrawTest}_verify_proofs PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PROOFS=1" FAIL_REGULAR_EXPRESSION "\\[draw-proofs\\] verify")
        list(APPEND agcDrawProofTests agc_driver_${agcVerifiedDrawTest}_verify_proofs)
    endforeach()
    set_tests_properties(${agcDrawProofTests} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(${agcDrawProofTests} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
