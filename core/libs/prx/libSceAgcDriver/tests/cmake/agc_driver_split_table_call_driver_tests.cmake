if(BUILD_TESTING)
    add_test(NAME agc_driver_split_table_call_driver_typed_gpu_rounds COMMAND agc_driver_split_table_call_driver_tests --typed-loads)
    add_test(NAME agc_driver_split_table_call_driver_typed_cpu_rounds COMMAND agc_driver_split_table_call_driver_tests --cpu-rounds --typed-loads)
    set_tests_properties(agc_driver_split_table_call_driver_typed_gpu_rounds agc_driver_split_table_call_driver_typed_cpu_rounds PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    set_tests_properties(agc_driver_split_table_call_driver_typed_gpu_rounds PROPERTIES
            PASS_REGULAR_EXPRESSION "a batch of [0-9]+ rounds ran [1-9][0-9]*.*split table call driver typed-load test passed")
    set_tests_properties(agc_driver_split_table_call_driver_typed_cpu_rounds PROPERTIES
            PASS_REGULAR_EXPRESSION "split table call driver typed-load test passed"
            FAIL_REGULAR_EXPRESSION "rounds selected on the GPU")
    if(WIN32)
        set_tests_properties(agc_driver_split_table_call_driver_typed_gpu_rounds agc_driver_split_table_call_driver_typed_cpu_rounds PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
