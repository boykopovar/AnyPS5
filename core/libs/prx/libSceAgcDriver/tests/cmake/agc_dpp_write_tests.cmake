add_test_executable(agc_dpp_write_tests EXCLUDE_FROM_ALL tests/DppWrite.cpp)
target_link_libraries(agc_dpp_write_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_dpp_write_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_dpp_write_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_dpp_write_tests)
endif()
