if(BUILD_TESTING)
    add_test_executable(agc_compute_pipeline_capture_tests tests/ComputePipelineCapture.cpp Execution/src/ComputePipelineCapture.cpp)
    target_include_directories(agc_compute_pipeline_capture_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(agc_compute_pipeline_capture_tests PRIVATE libc)
    set_target_properties(agc_compute_pipeline_capture_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_compute_pipeline_capture_tests)
    add_test(NAME agc_compute_pipeline_capture COMMAND agc_compute_pipeline_capture_tests)
    set_tests_properties(agc_compute_pipeline_capture PROPERTIES TIMEOUT 30)
    foreach(mode IN ITEMS entry-cap byte-cap io invalid)
        string(REPLACE "-" "_" name "${mode}")
        add_test(NAME agc_compute_pipeline_capture_optional_${name} COMMAND agc_compute_pipeline_capture_tests --optional-${mode})
        set_tests_properties(agc_compute_pipeline_capture_optional_${name} PROPERTIES TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_compute_pipeline_capture_optional_${name} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    if(WIN32)
        set_tests_properties(agc_compute_pipeline_capture PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING AND ANYPS5_ENABLE_SPIRV_TOOLS)
    add_test(NAME agc_compute_pipeline_capture_run COMMAND ${CMAKE_COMMAND}
            -DFIXTURE=$<TARGET_FILE:agc_driver_branch_past_endpgm_tests>
            -DREPLAY=$<TARGET_FILE:agc_compute_pipeline_replay>
            -DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/capture-tests
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/ComputePipelineCaptureRun.cmake)
    set_tests_properties(agc_compute_pipeline_capture_run PROPERTIES TIMEOUT 30 SKIP_REGULAR_EXPRESSION "compute pipeline capture run: skipped, no usable Vulkan device")
endif()
