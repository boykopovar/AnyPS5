add_test_executable(agc_input_twins_driver_tests EXCLUDE_FROM_ALL tests/InputTwins.cpp)
target_include_directories(agc_input_twins_driver_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_input_twins_driver_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_input_twins_driver_tests)

if(BUILD_TESTING)
    set_target_properties(agc_input_twins_driver_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_input_twins_driver COMMAND agc_input_twins_driver_tests)
endif()
