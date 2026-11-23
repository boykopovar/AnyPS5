if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_wait_attribution_trace)
    set_tests_properties(agc_driver_wait_attribution_trace agc_driver_wait_attribution_trace_draw_pipeline agc_driver_wait_attribution_trace_plan_verify PROPERTIES TIMEOUT 60)
    set_property(TEST agc_driver_wait_attribution_trace agc_driver_wait_attribution_trace_draw_pipeline agc_driver_wait_attribution_trace_plan_verify APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_FRAMES=unset:" "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:" "APS5_TRACE_TRIGGER_FILE=unset:" "APS5_TRACE_BENCH=unset:")
endif()
