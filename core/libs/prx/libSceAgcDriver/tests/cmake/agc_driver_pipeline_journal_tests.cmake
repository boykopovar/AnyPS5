target_include_directories(agc_driver_pipeline_journal_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(agc_driver_pipeline_journal_tests PRIVATE ${CMAKE_DL_LIBS})
if(BUILD_TESTING AND TARGET agc_driver_pipeline_journal_tests)
    agc_add_env_test(agc_driver_pipeline_journal_gpu_heap agc_driver_pipeline_journal_tests
            ARGS --gpu-heap ENV APS5_GPU_HEAP=1 TIMEOUT 30)
endif()
