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
        libkernel
        libc
)

set_target_properties(agc_driver_target_memo_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_target_memo_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_target_memo_tests)
    agc_add_draw_pipeline_test(agc_driver_target_memo)
    set_property(TEST agc_driver_target_memo agc_driver_target_memo_draw_pipeline APPEND PROPERTY ENVIRONMENT APS5_PROFILE_DRAW=1)
    set_property(TEST agc_driver_target_memo agc_driver_target_memo_draw_pipeline agc_driver_target_memo_plan_verify APPEND PROPERTY ENVIRONMENT APS5_NO_PASS_ATTACHMENT_MEMO=1)
    add_test(NAME agc_driver_target_memo_verify COMMAND agc_driver_target_memo_tests)
    add_test(NAME agc_driver_target_memo_disabled COMMAND agc_driver_target_memo_tests)
    foreach(mode IN ITEMS verify disabled)
        set_tests_properties(agc_driver_target_memo_${mode} PROPERTIES
                TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PROFILE_DRAW=1")
        if(WIN32)
            set_tests_properties(agc_driver_target_memo_${mode} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_property(TEST agc_driver_target_memo_verify APPEND PROPERTY ENVIRONMENT APS5_VERIFY_TARGET_MEMO=1 APS5_VERIFY_PASS_ATTACHMENTS=1)
    set_tests_properties(agc_driver_target_memo_verify PROPERTIES FAIL_REGULAR_EXPRESSION "differ from the full lookup")
    set_property(TEST agc_driver_target_memo_disabled APPEND PROPERTY ENVIRONMENT APS5_NO_TARGET_MEMO=1 APS5_NO_PASS_ATTACHMENT_MEMO=1)
    set_tests_properties(agc_driver_target_memo agc_driver_target_memo_draw_pipeline PROPERTIES TIMEOUT 30)
    foreach(scenario IN ITEMS descriptor msaa proof metadata eviction fallback baseline held-baseline switch-flip unchanged depth-proof claim untracked-dcc untracked-cmask untracked-stencil resident-dcc)
        foreach(mode IN ITEMS enabled disabled verify)
            set(test agc_driver_color_memo_${scenario}_${mode})
            add_test(NAME ${test} COMMAND agc_driver_target_memo_tests --r3 ${scenario})
            set_tests_properties(${test} PROPERTIES
                    TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PROFILE_DRAW=1")
            if(mode STREQUAL "disabled")
                set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT APS5_NO_TARGET_MEMO=1 APS5_NO_PASS_ATTACHMENT_MEMO=1)
            elseif(mode STREQUAL "verify")
                set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT APS5_VERIFY_TARGET_MEMO=1 APS5_VERIFY_PASS_ATTACHMENTS=1)
                set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "differ from the full lookup")
            else()
                set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT APS5_NO_PASS_ATTACHMENT_MEMO=1)
            endif()
            if(WIN32)
                set_tests_properties(${test} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
    foreach(entry IN ITEMS
            "target_only|off|on|APS5_NO_PASS_ATTACHMENT_MEMO=1"
            "pass_only|on|off|APS5_NO_TARGET_MEMO=1"
            "both|on|on|"
            "both_verify_pass|off|on|APS5_VERIFY_PASS_ATTACHMENTS=1"
            "both_verify_target|on|on|APS5_VERIFY_TARGET_MEMO=1"
            "both_verify|off|on|APS5_VERIFY_PASS_ATTACHMENTS=1;APS5_VERIFY_TARGET_MEMO=1")
        string(REPLACE "|" ";" fields "${entry}")
        list(GET fields 0 mode)
        list(GET fields 1 pass)
        list(GET fields 2 target)
        list(SUBLIST fields 3 -1 extra)
        set(test agc_driver_target_memo_order_${mode})
        add_test(NAME ${test} COMMAND agc_driver_target_memo_tests --r3 memo-order)
        set_tests_properties(${test} PROPERTIES
                TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PROFILE_DRAW=1;${extra}"
                PASS_REGULAR_EXPRESSION "memo order: pass attachment memo ${pass}, target memo ${target}.*target state memo tests passed"
                FAIL_REGULAR_EXPRESSION "differ from the full lookup")
        if(WIN32)
            set_tests_properties(${test} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_driver_target_memo_recipe_views COMMAND agc_driver_target_memo_tests --r3 recipe-views)
    set_tests_properties(agc_driver_target_memo_recipe_views PROPERTIES
            TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DRAW_PIPELINE=1;APS5_PROFILE_DRAW=1"
            PASS_REGULAR_EXPRESSION "recipe views: pass attachment memo on.*target state memo tests passed"
            FAIL_REGULAR_EXPRESSION "differ from the full lookup")
    if(WIN32)
        set_tests_properties(agc_driver_target_memo_recipe_views PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(NOT WIN32)
        add_test(NAME agc_driver_target_memo_watched_imports COMMAND agc_driver_target_memo_tests)
        set_tests_properties(agc_driver_target_memo_watched_imports PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PROFILE_DRAW=1;APS5_WRITE_WATCH_IMPORTS=watch;APS5_NO_PASS_ATTACHMENT_MEMO=1")
    endif()
endif()
