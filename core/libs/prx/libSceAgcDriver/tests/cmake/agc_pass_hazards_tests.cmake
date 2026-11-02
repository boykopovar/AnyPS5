add_test_executable(agc_pass_hazards_tests EXCLUDE_FROM_ALL tests/PassHazards.cpp Graphics/src/PassHazards.cpp)
target_include_directories(agc_pass_hazards_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(agc_pass_hazards_tests PRIVATE libc)
set_target_properties(agc_pass_hazards_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pass_hazards_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_pass_hazards_tests)
endif()
