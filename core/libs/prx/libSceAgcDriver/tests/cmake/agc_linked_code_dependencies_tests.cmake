if(BUILD_TESTING)
    add_test_executable(agc_linked_code_dependencies_tests tests/LinkedCodeDependencies.cpp)
    target_include_directories(agc_linked_code_dependencies_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
            ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(agc_linked_code_dependencies_tests PRIVATE libSceAgcDriver libc)
    set_target_properties(agc_linked_code_dependencies_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_linked_code_dependencies_tests)
    foreach(mode direct code row descriptor nested unused final-direct final-code final-row final-descriptor)
        add_test(NAME agc_linked_code_dependencies_${mode} COMMAND agc_linked_code_dependencies_tests ${mode})
        set_tests_properties(agc_linked_code_dependencies_${mode} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_linked_code_dependencies_${mode} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        add_test(NAME agc_linked_code_dependencies_${mode}_watched COMMAND agc_linked_code_dependencies_tests ${mode} --watched)
        set_tests_properties(agc_linked_code_dependencies_${mode}_watched PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_linked_code_dependencies_${mode}_watched PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        add_test(NAME agc_linked_code_dependencies_${mode}_stamp COMMAND agc_linked_code_dependencies_tests ${mode} --watched)
        set_tests_properties(agc_linked_code_dependencies_${mode}_stamp PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_DISPATCH_STAMP_VALIDATE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_linked_code_dependencies_${mode}_stamp PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    foreach(mode eviction disabled data-hit-verify data-hit-verify-table data-hit-verify-stale-data data-hit-verify-stale-code)
        add_test(NAME agc_linked_code_dependencies_${mode} COMMAND agc_linked_code_dependencies_tests ${mode})
        set_tests_properties(agc_linked_code_dependencies_${mode} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(mode MATCHES "^data-hit-verify")
            set_property(TEST agc_linked_code_dependencies_${mode} APPEND PROPERTY ENVIRONMENT "APS5_VERIFY_DATA_HITS=1")
        endif()
        if(mode MATCHES "^data-hit-verify-stale-")
            set_tests_properties(agc_linked_code_dependencies_${mode} PROPERTIES
                    PASS_REGULAR_EXPRESSION "APS5_VERIFY_DATA_HITS: the captured bytes disagrees")
        endif()
        if(WIN32)
            set_tests_properties(agc_linked_code_dependencies_${mode} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    foreach(mode hit-cost recover)
        foreach(validation watched stamp)
            add_test(NAME agc_linked_code_dependencies_${mode}_${validation} COMMAND agc_linked_code_dependencies_tests ${mode} --watched)
            set_tests_properties(agc_linked_code_dependencies_${mode}_${validation} PROPERTIES
                    ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
            if(validation STREQUAL "stamp")
                set_property(TEST agc_linked_code_dependencies_${mode}_${validation} APPEND PROPERTY ENVIRONMENT "APS5_DISPATCH_STAMP_VALIDATE=1")
            endif()
            if(WIN32)
                set_tests_properties(agc_linked_code_dependencies_${mode}_${validation} PROPERTIES
                        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
            endif()
        endforeach()
    endforeach()
    foreach(watched first table descriptor)
        add_test(NAME agc_linked_code_dependencies_mixed_${watched} COMMAND agc_linked_code_dependencies_tests row --watch-${watched})
        set_tests_properties(agc_linked_code_dependencies_mixed_${watched} PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_linked_code_dependencies_mixed_${watched} PROPERTIES
                    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
