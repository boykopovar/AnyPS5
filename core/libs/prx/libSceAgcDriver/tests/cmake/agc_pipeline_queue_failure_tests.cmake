add_test_executable(agc_pipeline_queue_failure_tests EXCLUDE_FROM_ALL tests/PipelineQueueFailure.cpp
    Graphics/src/PipelineLibrary.cpp tests/AllocationFailure.cpp)
target_include_directories(agc_pipeline_queue_failure_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_pipeline_queue_failure_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_pipeline_queue_failure_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pipeline_queue_failure_tests)

if(BUILD_TESTING)
    set_target_properties(agc_pipeline_queue_failure_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(mode wait clear stop)
        add_test(NAME agc_pipeline_queue_failure_${mode} COMMAND agc_pipeline_queue_failure_tests ${mode})
        set_tests_properties(agc_pipeline_queue_failure_${mode} PROPERTIES TIMEOUT 15)
        if(WIN32)
            set_tests_properties(agc_pipeline_queue_failure_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
