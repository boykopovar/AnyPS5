add_test_executable(agc_spirv_dump_tests EXCLUDE_FROM_ALL tests/SpirvDump.cpp)
target_link_libraries(agc_spirv_dump_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_spirv_dump_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_spirv_dump_tests)

if(ANYPS5_ENABLE_SPIRV_TOOLS)
    target_link_libraries(agc_spirv_dump_tests PRIVATE SPIRV-Tools-static)
endif()

if(BUILD_TESTING)
    agc_add_spirv_mode_tests(agc_spirv_dump_tests)
endif()
