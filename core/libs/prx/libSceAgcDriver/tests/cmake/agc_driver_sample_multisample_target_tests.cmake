target_include_directories(agc_driver_sample_multisample_target_tests PRIVATE $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>)

if(BUILD_TESTING)
    add_test(NAME agc_driver_native_fmask COMMAND agc_driver_sample_multisample_target_tests --native-fmask)
    set_tests_properties(agc_driver_native_fmask PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_native_fmask PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_native_msaa_vertex_input COMMAND agc_driver_sample_multisample_target_tests --native-vertex-input)
    set_tests_properties(agc_driver_native_msaa_vertex_input PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_native_msaa_vertex_input PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
