if(BUILD_TESTING)
    if(Python3_Interpreter_FOUND)
        add_test(NAME agc_driver_stencil_replay_parallel_atlas_guest_work COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/CheckPooledGuestWorkReport.py $<TARGET_FILE:agc_driver_stencil_replay_tests>)
        set_tests_properties(agc_driver_stencil_replay_parallel_atlas_guest_work PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PARALLEL_ATLAS=1;APS5_REPORT=1" SKIP_RETURN_CODE 77 TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_stencil_replay_parallel_atlas_guest_work PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>;PATH=path_list_prepend:$<TARGET_FILE_DIR:agc_driver_stencil_replay_tests>")
        endif()
    endif()
    add_test(NAME agc_driver_stencil_replay_no_gpl COMMAND agc_driver_stencil_replay_tests)
    set_tests_properties(agc_driver_stencil_replay_no_gpl PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_GPL=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_stencil_replay_parallel_atlas COMMAND agc_driver_stencil_replay_tests --pooled)
    set_tests_properties(agc_driver_stencil_replay_parallel_atlas PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PARALLEL_ATLAS=1;APS5_ASSERT_GPU_LOCK=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_stencil_replay_parallel_atlas_traced COMMAND agc_driver_stencil_replay_tests --traced)
    set_tests_properties(agc_driver_stencil_replay_parallel_atlas_traced PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PARALLEL_ATLAS=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    add_test(NAME agc_driver_stencil_replay_unsuppressed COMMAND agc_driver_stencil_replay_tests)
    set_tests_properties(agc_driver_stencil_replay_unsuppressed PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_GPL=1;APS5_NO_STATE_SUPPRESSION=1" TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_stencil_replay_no_gpl agc_driver_stencil_replay_unsuppressed agc_driver_stencil_replay_parallel_atlas agc_driver_stencil_replay_parallel_atlas_traced PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
