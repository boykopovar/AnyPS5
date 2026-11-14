if(BUILD_TESTING)
    add_test(NAME agc_driver_dispatch_overlap_off COMMAND agc_driver_dispatch_overlap_tests --off)
    set_tests_properties(agc_driver_dispatch_overlap_off PROPERTIES
        ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_DISPATCH_OVERLAP=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_dispatch_overlap_off PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    agc_add_env_test(agc_driver_dispatch_overlap_recipe_verify agc_driver_dispatch_overlap_tests TIMEOUT 60
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_VERIFY_RECIPE=1)
    agc_add_env_test(agc_driver_dispatch_overlap_recipe_verify_gpu_heap agc_driver_dispatch_overlap_tests TIMEOUT 60
            ENV ANYPS5_NO_SHADER_CACHE=1 APS5_GPU_HEAP=1 APS5_VERIFY_GPU_HEAP=1 APS5_VERIFY_RECIPE=1)
endif()
