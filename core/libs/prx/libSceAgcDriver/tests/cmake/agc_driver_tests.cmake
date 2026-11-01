add_test_executable(agc_driver_tests EXCLUDE_FROM_ALL
        tests/Submit.cpp
)

target_include_directories(agc_driver_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_tests PRIVATE
        libSceAgcDriver
        libkernel
        libc
)

set_target_properties(agc_driver_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_tests)

if(BUILD_TESTING)
    add_test(NAME agc_driver_readonly_resummarize COMMAND agc_driver_tests --readonly-resummarize)
    set_tests_properties(agc_driver_readonly_resummarize PROPERTIES TIMEOUT 10)
    add_test(NAME agc_driver_eop_interrupts COMMAND agc_driver_tests --eop-interrupts)
    set_tests_properties(agc_driver_eop_interrupts PROPERTIES TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_readonly_resummarize agc_driver_eop_interrupts PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_small_copy_sync COMMAND agc_driver_tests --small-copy-sync)
    set_tests_properties(agc_driver_small_copy_sync PROPERTIES TIMEOUT 30 ENVIRONMENT "APS5_COPY_SYNC=1" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_small_copy_sync PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    agc_add_default_test(agc_driver_tests)
    set_tests_properties(agc_driver_draw_pipeline_trace PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=8;APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=64" TIMEOUT 30)
    set_property(TEST agc_driver_draw_pipeline_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    add_test(NAME agc_driver_draw_pipeline_trace_disabled COMMAND ${CMAKE_COMMAND} -E env
            --unset=APS5_TRACE_FRAMES --unset=APS5_TRACE_SECONDS --unset=APS5_TRACE_SUMMARY_EVERY
            $<TARGET_FILE:agc_driver_draw_pipeline_trace_tests> disabled)
    set_tests_properties(agc_driver_draw_pipeline_trace_disabled PROPERTIES ENVIRONMENT "APS5_PIPELINED_DRAWS=8" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_driver_draw_finisher_trace COMMAND agc_driver_draw_pipeline_trace_tests finisher)
    set_tests_properties(agc_driver_draw_finisher_trace PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=8;APS5_TRACE_FRAMES=0:1;APS5_TRACE_EVENTS=64" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_draw_finisher_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    add_test(NAME agc_driver_draw_finisher_trace_disabled COMMAND ${CMAKE_COMMAND} -E env
            --unset=APS5_TRACE_FRAMES --unset=APS5_TRACE_SECONDS --unset=APS5_TRACE_SUMMARY_EVERY
            $<TARGET_FILE:agc_driver_draw_pipeline_trace_tests> finisher-disabled)
    set_tests_properties(agc_driver_draw_finisher_trace_disabled PROPERTIES ENVIRONMENT "APS5_DRAW_PIPELINE=1;APS5_PIPELINED_DRAWS=8" SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_draw_pipeline_trace_disabled agc_driver_draw_finisher_trace agc_driver_draw_finisher_trace_disabled PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
