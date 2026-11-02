add_test_executable(agc_driver_bda_device_tests EXCLUDE_FROM_ALL
        tests/BdaDevice.cpp
        tests/BdaShader.cpp
        tests/BdaExecution.cpp
        tests/BdaContracts.cpp
        tests/HostImportFailure.cpp
)

target_include_directories(agc_driver_bda_device_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_driver_bda_device_tests PRIVATE libSceAgcDriver SDL2-static libc)
set_target_properties(agc_driver_bda_device_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_bda_device_tests)

if(BUILD_TESTING)
    add_test(NAME agc_driver_host_import_failure_handles COMMAND agc_driver_bda_device_tests --host-import-failures)
    set_tests_properties(agc_driver_host_import_failure_handles PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_host_import_failure_handles PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_bda_device_tests)
    add_test(NAME agc_driver_bda_device_selection_cpu COMMAND agc_driver_bda_device_tests --device-selection-cpu)
    set_tests_properties(agc_driver_bda_device_selection_cpu PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_bda_device_selection_cpu PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
