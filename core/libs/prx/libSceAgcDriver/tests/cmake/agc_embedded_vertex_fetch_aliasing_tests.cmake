add_test_executable(agc_embedded_vertex_fetch_aliasing_tests EXCLUDE_FROM_ALL tests/EmbeddedVertexFetchAliasing.cpp)
target_link_libraries(agc_embedded_vertex_fetch_aliasing_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_embedded_vertex_fetch_aliasing_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_embedded_vertex_fetch_aliasing_tests)

if(BUILD_TESTING)
    set_target_properties(agc_embedded_vertex_fetch_aliasing_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_embedded_vertex_fetch_aliasing COMMAND agc_embedded_vertex_fetch_aliasing_tests)
    if(WIN32)
        set_tests_properties(agc_embedded_vertex_fetch_aliasing PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
