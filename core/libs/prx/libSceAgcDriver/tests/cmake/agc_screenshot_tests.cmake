add_test_executable(agc_screenshot_tests EXCLUDE_FROM_ALL tests/Screenshot.cpp Execution/src/Screenshot.cpp)
target_include_directories(agc_screenshot_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_screenshot_tests PRIVATE decoder_png SDL2-static libc)
set_target_properties(agc_screenshot_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_screenshot_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_screenshot_tests)
endif()
