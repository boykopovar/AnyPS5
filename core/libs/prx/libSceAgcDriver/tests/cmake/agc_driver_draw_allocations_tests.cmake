if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_draw_allocations)
    set_property(TEST agc_driver_draw_allocations agc_driver_draw_allocations_draw_pipeline agc_driver_draw_allocations_plan_verify APPEND PROPERTY ENVIRONMENT_MODIFICATION "VK_INSTANCE_LAYERS=unset:")
endif()
