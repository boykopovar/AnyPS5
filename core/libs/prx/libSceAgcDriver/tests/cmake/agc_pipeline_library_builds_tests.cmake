add_test_executable(agc_pipeline_library_builds_tests EXCLUDE_FROM_ALL tests/PipelineLibraryBuilds.cpp
    Graphics/src/PipelineLibrary.cpp)
target_include_directories(agc_pipeline_library_builds_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_pipeline_library_builds_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_pipeline_library_builds_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pipeline_library_builds_tests)

if(BUILD_TESTING)
    set_target_properties(agc_pipeline_library_builds_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(mode shared independent failure exception clear)
        add_test(NAME agc_pipeline_library_builds_${mode} COMMAND agc_pipeline_library_builds_tests ${mode})
        set_tests_properties(agc_pipeline_library_builds_${mode} PROPERTIES TIMEOUT 15)
        if(WIN32)
            set_tests_properties(agc_pipeline_library_builds_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
