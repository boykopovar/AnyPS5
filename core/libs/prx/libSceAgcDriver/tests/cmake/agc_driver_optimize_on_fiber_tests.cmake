if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_link_libraries(agc_driver_optimize_on_fiber_tests PRIVATE libSceFiber shader_recompiler_agc_driver)
endif()
