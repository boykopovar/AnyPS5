add_test_executable(agc_vertex_input_tests EXCLUDE_FROM_ALL tests/VertexInput.cpp)
target_include_directories(agc_vertex_input_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(agc_vertex_input_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vertex_input_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vertex_input_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_vertex_input_tests)
endif()
