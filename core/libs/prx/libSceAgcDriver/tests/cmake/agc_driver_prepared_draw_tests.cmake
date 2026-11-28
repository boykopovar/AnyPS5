if(BUILD_TESTING)
    add_test(NAME agc_driver_prepared_draw_plan_verify COMMAND agc_driver_prepared_draw_tests)
    set_tests_properties(agc_driver_prepared_draw_plan_verify PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PLAN=1;APS5_VERIFY_EARLY_DRAW_KEY=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_prepared_draw_content_key COMMAND agc_driver_prepared_draw_tests)
    set_tests_properties(agc_driver_prepared_draw_content_key PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PLAN=1;APS5_NO_DRAW_KEY_TRIM=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_prepared_draw_late_key COMMAND agc_driver_prepared_draw_tests)
    set_tests_properties(agc_driver_prepared_draw_late_key PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PLAN=1;APS5_VERIFY_EARLY_DRAW_KEY=1;APS5_NO_EARLY_DRAW_KEY=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_prepared_draw_late_key_untrimmed COMMAND agc_driver_prepared_draw_tests)
    set_tests_properties(agc_driver_prepared_draw_late_key_untrimmed PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_PLAN=1;APS5_NO_EARLY_DRAW_KEY=1;APS5_NO_DRAW_KEY_TRIM=1;APS5_VERIFY_EARLY_DRAW_KEY=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_draw_cache_reuse_finisher_prepare COMMAND agc_driver_draw_cache_reuse_tests --finisher-prepare)
    set_tests_properties(agc_driver_draw_cache_reuse_finisher_prepare PROPERTIES SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "FATAL:" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1;APS5_VERIFY_DRAW_PLAN=1;APS5_VERIFY_DRAW_RECIPE=1;APS5_VERIFY_REGISTER_KEYS=1")
    add_test(NAME agc_driver_draw_cache_reuse_late_key COMMAND agc_driver_draw_cache_reuse_tests --finisher-prepare)
    set_tests_properties(agc_driver_draw_cache_reuse_late_key PROPERTIES SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "FATAL:" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1;APS5_NO_EARLY_DRAW_KEY=1;APS5_VERIFY_EARLY_DRAW_KEY=1;APS5_VERIFY_DRAW_PLAN=1;APS5_VERIFY_DRAW_RECIPE=1;APS5_VERIFY_REGISTER_KEYS=1")
    add_test(NAME agc_driver_draw_cache_reuse_late_key_untrimmed COMMAND agc_driver_draw_cache_reuse_tests --finisher-prepare)
    set_tests_properties(agc_driver_draw_cache_reuse_late_key_untrimmed PROPERTIES SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "FATAL:" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1;APS5_NO_EARLY_DRAW_KEY=1;APS5_NO_DRAW_KEY_TRIM=1;APS5_VERIFY_EARLY_DRAW_KEY=1;APS5_VERIFY_DRAW_PLAN=1;APS5_VERIFY_DRAW_RECIPE=1;APS5_VERIFY_REGISTER_KEYS=1")
    add_test(NAME agc_driver_draw_cache_reuse_no_finisher_prepare COMMAND agc_driver_draw_cache_reuse_tests --no-finisher-prepare)
    set_tests_properties(agc_driver_draw_cache_reuse_no_finisher_prepare PROPERTIES SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "FATAL:" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1;APS5_NO_FINISHER_PREPARE=1;APS5_VERIFY_DRAW_PLAN=1;APS5_VERIFY_DRAW_RECIPE=1;APS5_VERIFY_REGISTER_KEYS=1")
    add_test(NAME agc_driver_draw_cache_reuse_finisher_prepare_switch COMMAND agc_driver_draw_cache_reuse_tests --finisher-prepare-switch)
    set_tests_properties(agc_driver_draw_cache_reuse_finisher_prepare_switch PROPERTIES SKIP_RETURN_CODE 77 FAIL_REGULAR_EXPRESSION "FATAL:" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=32;APS5_PIPELINE_DISPATCH=1;APS5_NO_FINISHER_PREPARE=1;APS5_VERIFY_DRAW_PLAN=1")
    if(WIN32)
        set_tests_properties(agc_driver_prepared_draw_plan_verify agc_driver_prepared_draw_content_key agc_driver_prepared_draw_late_key agc_driver_prepared_draw_late_key_untrimmed agc_driver_draw_cache_reuse_finisher_prepare agc_driver_draw_cache_reuse_late_key agc_driver_draw_cache_reuse_late_key_untrimmed agc_driver_draw_cache_reuse_no_finisher_prepare agc_driver_draw_cache_reuse_finisher_prepare_switch PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
