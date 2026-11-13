add_test_executable(agc_device_image_keys_tests EXCLUDE_FROM_ALL tests/DeviceImageKeys.cpp Graphics/src/DeviceImageKeys.cpp)
target_include_directories(agc_device_image_keys_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler)
target_link_libraries(agc_device_image_keys_tests PRIVATE libc)
set_target_properties(agc_device_image_keys_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_device_image_keys_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_device_image_keys_tests)
endif()
