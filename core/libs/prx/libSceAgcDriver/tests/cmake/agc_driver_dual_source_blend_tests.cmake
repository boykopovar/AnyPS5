target_include_directories(agc_driver_dual_source_blend_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

if(BUILD_TESTING)
    add_test(NAME agc_driver_headless_present_mode COMMAND $<TARGET_FILE:agc_driver_dual_source_blend_tests>)
    set_tests_properties(agc_driver_headless_present_mode PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PIPELINE_JOURNAL=1;ANYPS5_PRESENT_MODE=fifo_latest_ready"
            FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-|was not found but was requested by env var VK_INSTANCE_LAYERS"
            SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_headless_present_mode PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
