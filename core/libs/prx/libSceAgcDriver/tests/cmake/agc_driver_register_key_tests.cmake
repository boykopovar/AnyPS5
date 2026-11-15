add_test_executable(agc_driver_register_key_tests EXCLUDE_FROM_ALL tests/RegisterKey.cpp)
target_include_directories(agc_driver_register_key_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_register_key_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)
set_target_properties(agc_driver_register_key_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_register_key_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_register_key_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_register_key COMMAND agc_driver_register_key_tests)
    set_tests_properties(agc_driver_register_key PROPERTIES TIMEOUT 30 ENVIRONMENT "APS5_VERIFY_REGISTER_KEYS=1")
    if(WIN32)
        set_tests_properties(agc_driver_register_key PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
