add_test_executable(agc_interlock_epilog_tests EXCLUDE_FROM_ALL tests/InterlockEpilog.cpp)
target_include_directories(agc_interlock_epilog_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
set_target_properties(agc_interlock_epilog_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_interlock_epilog_tests)

if(BUILD_TESTING)
    set_target_properties(agc_interlock_epilog_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_interlock_epilog COMMAND agc_interlock_epilog_tests)
endif()
