add_test_executable(agc_barrier_batch_tests EXCLUDE_FROM_ALL tests/BarrierBatch.cpp Graphics/src/BarrierBatch.cpp)
target_include_directories(agc_barrier_batch_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_barrier_batch_tests PRIVATE libc)
set_target_properties(agc_barrier_batch_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_barrier_batch_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_barrier_batch_tests)
endif()
