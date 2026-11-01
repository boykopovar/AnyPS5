if(BUILD_TESTING)
    add_test_executable(sampler_parameter_bounds_tests ${LIBS_INCLUDE_DIR}/tests/SamplerParameterBounds.cpp Graphics/src/Sampler.cpp Graphics/src/GuestSamplerResource.cpp Graphics/src/TextureFormat.cpp)
    target_include_directories(sampler_parameter_bounds_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Graphics/include
        ${LIBS_INCLUDE_DIR}
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/IntermediateRepresentation/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include)
    target_link_libraries(sampler_parameter_bounds_tests PRIVATE libc)
    configure_windows_unwind(sampler_parameter_bounds_tests)
    add_test(NAME sampler_parameter_bounds COMMAND sampler_parameter_bounds_tests)
    set_tests_properties(sampler_parameter_bounds PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(sampler_parameter_bounds PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
