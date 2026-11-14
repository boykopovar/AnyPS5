add_test_executable(agc_exclusive_scratch_tests EXCLUDE_FROM_ALL tests/ExclusiveScratch.cpp)
target_include_directories(agc_exclusive_scratch_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(agc_exclusive_scratch_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_exclusive_scratch_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_exclusive_scratch_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_exclusive_scratch_tests)
endif()
