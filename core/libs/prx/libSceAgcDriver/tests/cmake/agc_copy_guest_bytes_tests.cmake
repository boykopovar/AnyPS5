add_test_executable(agc_copy_guest_bytes_tests EXCLUDE_FROM_ALL tests/CopyGuestBytes.cpp)
target_include_directories(agc_copy_guest_bytes_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_copy_guest_bytes_tests PRIVATE libSceAgcDriver libc)
set_target_properties(agc_copy_guest_bytes_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_copy_guest_bytes_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_copy_guest_bytes_tests)
endif()
