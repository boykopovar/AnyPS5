if(BUILD_TESTING)
    add_test_executable(agc_gpu_trace_capacity_tests tests/GpuTraceCapacity.cpp)
    target_include_directories(agc_gpu_trace_capacity_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
            ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
            ${agcZlibIncludeDir}
    )
    target_link_libraries(agc_gpu_trace_capacity_tests PRIVATE libSceAgcDriver SDL2-static libc jpeg_turbo)
    set_target_properties(agc_gpu_trace_capacity_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_gpu_trace_capacity_tests)
    add_test(NAME agc_gpu_trace_capacity_small COMMAND agc_gpu_trace_capacity_tests 1)
    add_test(NAME agc_gpu_trace_capacity_large COMMAND agc_gpu_trace_capacity_tests 262145)
    set_tests_properties(agc_gpu_trace_capacity_small agc_gpu_trace_capacity_large PROPERTIES
            TIMEOUT 20
            SKIP_RETURN_CODE 77
    )
    set_tests_properties(agc_gpu_trace_capacity_small PROPERTIES
            ENVIRONMENT "APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=1"
    )
    set_tests_properties(agc_gpu_trace_capacity_large PROPERTIES
            ENVIRONMENT "APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=262145"
    )
    if(WIN32)
        set_tests_properties(agc_gpu_trace_capacity_small agc_gpu_trace_capacity_large PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>")
    endif()
endif()
