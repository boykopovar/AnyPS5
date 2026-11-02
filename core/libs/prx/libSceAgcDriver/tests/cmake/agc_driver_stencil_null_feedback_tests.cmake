if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_compile_definitions(agc_driver_stencil_null_feedback_tests PRIVATE ANYPS5_ENABLE_SPIRV_TOOLS=1)
    target_link_libraries(agc_driver_stencil_null_feedback_tests PRIVATE SPIRV-Tools-static)
endif()
