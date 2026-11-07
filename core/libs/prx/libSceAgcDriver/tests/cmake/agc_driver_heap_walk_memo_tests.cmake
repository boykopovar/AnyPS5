add_test_executable(agc_driver_heap_walk_memo_tests EXCLUDE_FROM_ALL
        tests/HeapWalkMemo.cpp
)

target_include_directories(agc_driver_heap_walk_memo_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_heap_walk_memo_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_heap_walk_memo_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_heap_walk_memo_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_heap_walk_memo_tests)
endif()
