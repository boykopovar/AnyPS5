add_test_executable(agc_lds_lock_budget_tests EXCLUDE_FROM_ALL tests/LdsLockBudget.cpp)
target_link_libraries(agc_lds_lock_budget_tests PRIVATE shader_recompiler_agc_driver libc)
set_target_properties(agc_lds_lock_budget_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_lds_lock_budget_tests)

if(BUILD_TESTING)
    set_target_properties(agc_lds_lock_budget_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_lds_lock_budget COMMAND agc_lds_lock_budget_tests)
    set_tests_properties(agc_lds_lock_budget PROPERTIES TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_lds_lock_budget PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
