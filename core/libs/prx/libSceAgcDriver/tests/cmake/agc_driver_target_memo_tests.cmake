add_test_executable(agc_driver_target_memo_tests EXCLUDE_FROM_ALL
        tests/TargetStateMemo.cpp
        ${agcCoverShaderHeaders}
)

target_include_directories(agc_driver_target_memo_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_target_memo_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_target_memo_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_target_memo_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_target_memo_tests)
    agc_add_draw_pipeline_test(agc_driver_target_memo)
    set_property(TEST agc_driver_target_memo agc_driver_target_memo_draw_pipeline APPEND PROPERTY ENVIRONMENT APS5_PROFILE_DRAW=1)
    add_test(NAME agc_driver_target_memo_verify COMMAND agc_driver_target_memo_tests)
    add_test(NAME agc_driver_target_memo_disabled COMMAND agc_driver_target_memo_tests)
    foreach(mode IN ITEMS verify disabled)
        set_tests_properties(agc_driver_target_memo_${mode} PROPERTIES
                TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PROFILE_DRAW=1")
        if(WIN32)
            set_tests_properties(agc_driver_target_memo_${mode} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_property(TEST agc_driver_target_memo_verify APPEND PROPERTY ENVIRONMENT APS5_VERIFY_TARGET_MEMO=1)
    set_property(TEST agc_driver_target_memo_disabled APPEND PROPERTY ENVIRONMENT APS5_NO_TARGET_MEMO=1)
    set_tests_properties(agc_driver_target_memo agc_driver_target_memo_draw_pipeline PROPERTIES TIMEOUT 30)
    foreach(scenario IN ITEMS descriptor msaa proof metadata eviction fallback baseline held-baseline unchanged depth-proof claim untracked-dcc untracked-cmask resident-dcc)
        foreach(mode IN ITEMS enabled disabled verify)
            set(test agc_driver_color_memo_${scenario}_${mode})
            add_test(NAME ${test} COMMAND agc_driver_target_memo_tests --r3 ${scenario})
            set_tests_properties(${test} PROPERTIES
                    TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PROFILE_DRAW=1")
            if(mode STREQUAL "disabled")
                set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT APS5_NO_TARGET_MEMO=1)
            elseif(mode STREQUAL "verify")
                set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT APS5_VERIFY_TARGET_MEMO=1)
            endif()
            if(WIN32)
                set_tests_properties(${test} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
endif()
