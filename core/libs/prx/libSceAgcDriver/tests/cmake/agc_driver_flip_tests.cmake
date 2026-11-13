add_test_executable(agc_driver_flip_tests EXCLUDE_FROM_ALL
        tests/Flip.cpp
)

target_include_directories(agc_driver_flip_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)

target_link_libraries(agc_driver_flip_tests PRIVATE
        libSceAgcDriver
        libc
)

set_target_properties(agc_driver_flip_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_flip_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_flip_tests)
    add_test(NAME agc_driver_present_mode COMMAND agc_driver_flip_tests present-mode)
    add_test(NAME agc_driver_worker_failure_log COMMAND agc_driver_flip_tests failure-log)
    set_tests_properties(agc_driver_worker_failure_log PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 20)
    add_test(NAME agc_driver_flip_compute COMMAND agc_driver_flip_tests compute)
    set_tests_properties(agc_driver_flip_compute PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_flip_compute agc_driver_present_mode agc_driver_worker_failure_log PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
