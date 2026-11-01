add_test_executable(agc_vertex_barrier_tests EXCLUDE_FROM_ALL tests/VertexBarrier.cpp)
target_link_libraries(agc_vertex_barrier_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_vertex_barrier_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_vertex_barrier_tests)

if(BUILD_TESTING)
    set_target_properties(agc_vertex_barrier_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_vertex_barrier COMMAND agc_vertex_barrier_tests)
    set_tests_properties(agc_vertex_barrier PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_vertex_barrier PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
