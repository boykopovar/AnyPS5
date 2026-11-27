if(BUILD_TESTING)
    add_test(NAME agc_driver_draw_recapture_on_change_inline COMMAND agc_driver_draw_recapture_on_change_tests)
    set_tests_properties(agc_driver_draw_recapture_on_change_inline PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=0" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_draw_recapture_on_change_finisher COMMAND agc_driver_draw_recapture_on_change_tests)
    set_tests_properties(agc_driver_draw_recapture_on_change_finisher PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_dispatch_recapture_on_change_items COMMAND agc_driver_dispatch_recapture_on_change_tests)
    set_tests_properties(agc_driver_dispatch_recapture_on_change_items PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=1;APS5_PIPELINE_DISPATCH=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_recorded_draw_stage_groups
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:agc_driver_draw_recapture_on_change_tests>
            -P "${CMAKE_CURRENT_LIST_DIR}/../RecordedDrawStageGroups.cmake")
    set_tests_properties(agc_driver_recorded_draw_stage_groups PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=0;APS5_PROFILE_GPU=1;APS5_PROFILE_GPU_DRAWS=1;APS5_PROFILE_GPU_DRAW_STAGES=1"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60
        SKIP_REGULAR_EXPRESSION "recorded draw stage groups: Vulkan unavailable"
        FAIL_REGULAR_EXPRESSION "VUID-|SYNC-HAZARD")
    if(WIN32)
        set_tests_properties(agc_driver_draw_recapture_on_change_inline agc_driver_draw_recapture_on_change_finisher agc_driver_dispatch_recapture_on_change_items agc_driver_recorded_draw_stage_groups PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
