if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_draw_handoff_spin)
    set_tests_properties(agc_driver_draw_handoff_spin agc_driver_draw_handoff_spin_draw_pipeline agc_driver_draw_handoff_spin_plan_verify PROPERTIES TIMEOUT 60)
endif()
