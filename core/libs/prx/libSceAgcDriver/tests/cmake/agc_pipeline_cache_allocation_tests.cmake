add_test_executable(agc_pipeline_cache_allocation_tests EXCLUDE_FROM_ALL tests/PipelineCacheFailures.cpp
    Graphics/src/PipelineCache.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_pipeline_cache_allocation_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_pipeline_cache_allocation_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_pipeline_cache_allocation_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pipeline_cache_allocation_tests)

if(BUILD_TESTING)
    set_target_properties(agc_pipeline_cache_allocation_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(mode startup now busy destructor worker)
        add_test(NAME agc_pipeline_cache_allocation_${mode} COMMAND agc_pipeline_cache_allocation_tests ${mode})
        set_tests_properties(agc_pipeline_cache_allocation_${mode} PROPERTIES TIMEOUT 25)
        if(WIN32)
            set_tests_properties(agc_pipeline_cache_allocation_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    if(WIN32)
        add_test(NAME agc_pipeline_cache_read_retry COMMAND agc_pipeline_cache_allocation_tests read-retry)
        set_tests_properties(agc_pipeline_cache_read_retry PROPERTIES TIMEOUT 25 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
