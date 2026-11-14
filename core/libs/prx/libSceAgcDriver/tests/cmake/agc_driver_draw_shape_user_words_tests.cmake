if(BUILD_TESTING)
    agc_add_draw_pipeline_test(agc_driver_draw_shape_user_words)
    add_test(NAME agc_driver_draw_shape_user_words_verify COMMAND agc_driver_draw_shape_user_words_tests)
    set_tests_properties(agc_driver_draw_shape_user_words_verify PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_DATA_HITS=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_draw_shape_user_words_off COMMAND agc_driver_draw_shape_user_words_tests)
    add_test(NAME agc_driver_draw_data_hit_user_words_shape_off COMMAND agc_driver_draw_data_hit_user_words_tests)
    add_test(NAME agc_driver_draw_cache_reuse_shape_off COMMAND agc_driver_draw_cache_reuse_tests)
    add_test(NAME agc_driver_draw_cache_relocation_shape_off COMMAND agc_driver_draw_cache_relocation_tests)
    set_tests_properties(agc_driver_draw_shape_user_words_off agc_driver_draw_data_hit_user_words_shape_off agc_driver_draw_cache_reuse_shape_off agc_driver_draw_cache_relocation_shape_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_SHAPE_USER_WORDS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_draw_shape_user_words_off_verify COMMAND agc_driver_draw_shape_user_words_tests)
    add_test(NAME agc_driver_draw_data_hit_user_words_shape_off_verify COMMAND agc_driver_draw_data_hit_user_words_tests)
    set_tests_properties(agc_driver_draw_shape_user_words_off_verify agc_driver_draw_data_hit_user_words_shape_off_verify PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_SHAPE_USER_WORDS=1;APS5_VERIFY_DRAW_DATA_HITS=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_draw_data_hit_user_words_shape_off_refresh COMMAND agc_driver_draw_data_hit_user_words_tests)
    set_tests_properties(agc_driver_draw_data_hit_user_words_shape_off_refresh PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DRAW_SHAPE_USER_WORDS=1;APS5_DRAW_PIPELINE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_draw_shape_user_words_vertex_off COMMAND agc_driver_draw_shape_user_words_tests)
    set_tests_properties(agc_driver_draw_shape_user_words_vertex_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_SHAPE_VERTEX_INPUT_STAGES=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_draw_shape_user_words_verify agc_driver_draw_shape_user_words_off agc_driver_draw_data_hit_user_words_shape_off agc_driver_draw_cache_reuse_shape_off agc_driver_draw_cache_relocation_shape_off agc_driver_draw_shape_user_words_off_verify agc_driver_draw_data_hit_user_words_shape_off_verify agc_driver_draw_data_hit_user_words_shape_off_refresh agc_driver_draw_shape_user_words_vertex_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
