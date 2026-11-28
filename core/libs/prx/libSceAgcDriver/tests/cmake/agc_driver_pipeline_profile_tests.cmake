if(BUILD_TESTING)
    add_test_executable(agc_driver_pipeline_profile_tests tests/DrawPipelineProfile.cpp)
    target_include_directories(agc_driver_pipeline_profile_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
    target_link_libraries(agc_driver_pipeline_profile_tests PRIVATE libSceAgcDriver)
    set_target_properties(agc_driver_pipeline_profile_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_driver_pipeline_profile_tests)
    add_test(NAME agc_driver_pipeline_profile COMMAND agc_driver_pipeline_profile_tests)
    set_tests_properties(agc_driver_pipeline_profile PROPERTIES TIMEOUT 20
        ENVIRONMENT "APS5_PIPELINE_STATS=1;APS5_PIPELINED_DRAWS=8")
    add_test(NAME agc_driver_pipeline_untimed COMMAND agc_driver_pipeline_profile_tests --untimed)
    set_tests_properties(agc_driver_pipeline_untimed PROPERTIES TIMEOUT 20
        ENVIRONMENT "APS5_PIPELINED_DRAWS=8")
    if(WIN32)
        set_tests_properties(agc_driver_pipeline_profile agc_driver_pipeline_untimed PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>")
    endif()
    set_property(TEST agc_driver_pipeline_untimed APPEND PROPERTY ENVIRONMENT_MODIFICATION "APS5_PROFILE_DRAW=unset:" "APS5_PIPELINE_STATS=unset:")
    add_test(NAME agc_driver_decode_drain_skip COMMAND agc_driver_pipeline_profile_tests decode-drain-skip)
    set_tests_properties(agc_driver_decode_drain_skip PROPERTIES TIMEOUT 20
        ENVIRONMENT "APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=8")
    if(WIN32)
        set_tests_properties(agc_driver_decode_drain_skip PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAgcDriver>")
    endif()
endif()
