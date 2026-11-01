if(BUILD_TESTING AND ANYPS5_ENABLE_SPIRV_TOOLS)
    add_test_executable(agc_compute_pipeline_replay_tests tests/ComputePipelineReplay.cpp)
    target_include_directories(agc_compute_pipeline_replay_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
    )
    target_link_libraries(agc_compute_pipeline_replay_tests PRIVATE libSceAgcDriver libc)
    set_target_properties(agc_compute_pipeline_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_compute_pipeline_replay_tests)
    add_test(NAME agc_compute_pipeline_replay COMMAND agc_compute_pipeline_replay_tests)
    set_tests_properties(agc_compute_pipeline_replay PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_dependencies(agc_compute_pipeline_replay_tests agc_compute_pipeline_replay agc_driver_branch_past_endpgm_tests)
endif()
