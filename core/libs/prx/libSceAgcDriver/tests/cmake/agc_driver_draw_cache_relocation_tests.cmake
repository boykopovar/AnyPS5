target_include_directories(agc_driver_draw_cache_relocation_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

if(BUILD_TESTING)
    add_test(NAME agc_driver_draw_cache_relocation_recipe_verify COMMAND agc_driver_draw_cache_relocation_tests)
    set_tests_properties(agc_driver_draw_cache_relocation_recipe_verify PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_VERIFY_DRAW_RECIPE=1;APS5_PIPELINED_DRAWS=0" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_draw_cache_relocation_evicted COMMAND agc_driver_draw_cache_relocation_tests --evicted)
    add_test(NAME agc_driver_draw_cache_relocation_kept COMMAND agc_driver_draw_cache_relocation_tests --kept)
    set_tests_properties(agc_driver_draw_cache_relocation_evicted agc_driver_draw_cache_relocation_kept PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_CACHE_ENTRIES=2" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_draw_cache_relocation_recipe_verify agc_driver_draw_cache_relocation_evicted agc_driver_draw_cache_relocation_kept PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    agc_add_draw_pipeline_test(agc_driver_draw_cache_relocation)
endif()
