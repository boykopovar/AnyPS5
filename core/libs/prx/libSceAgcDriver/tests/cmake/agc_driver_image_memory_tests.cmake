if(BUILD_TESTING)
    if(NOT ANYPS5_AGC_EXECUTION_TESTS OR "ImageMemory" IN_LIST ANYPS5_AGC_EXECUTION_TESTS)
        add_test(NAME agc_driver_image_memory_hooks COMMAND agc_driver_image_memory_tests --check-hooks)
        set_tests_properties(agc_driver_image_memory_hooks PROPERTIES TIMEOUT 30)
        if(Python3_FOUND)
            foreach(mode IN ITEMS images pressure)
                foreach(teardown IN ITEMS normal failure)
                    set(skip_args --dedicated-${mode})
                    if(teardown STREQUAL "failure")
                        list(APPEND skip_args --teardown-failure)
                    endif()
                    add_test(NAME agc_driver_image_memory_dedicated_${mode}_${teardown}_skip
                            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/execution/CheckImageMemorySkip.py
                            $<TARGET_FILE:agc_driver_image_memory_tests> ${skip_args})
                    set_tests_properties(agc_driver_image_memory_dedicated_${mode}_${teardown}_skip PROPERTIES
                            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
                    if(WIN32)
                        set_tests_properties(agc_driver_image_memory_dedicated_${mode}_${teardown}_skip PROPERTIES ENVIRONMENT_MODIFICATION
                                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
                    endif()
                endforeach()
            endforeach()
        endif()
        if(WIN32)
            set_tests_properties(agc_driver_image_memory_hooks PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
    set_tests_properties(agc_driver_image_memory PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_image_memory_no_slabs COMMAND agc_driver_image_memory_tests --no-sharing-check)
    set_tests_properties(agc_driver_image_memory_no_slabs PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_image_memory_no_slabs APPEND PROPERTY ENVIRONMENT "APS5_NO_IMAGE_SLABS=1")
    add_test(NAME agc_driver_image_memory_teardown_failure COMMAND agc_driver_image_memory_tests --teardown-failure)
    add_test(NAME agc_driver_image_memory_no_slabs_teardown_failure COMMAND agc_driver_image_memory_tests --no-sharing-check --teardown-failure)
    set_tests_properties(agc_driver_image_memory_teardown_failure agc_driver_image_memory_no_slabs_teardown_failure PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_property(TEST agc_driver_image_memory_no_slabs_teardown_failure APPEND PROPERTY ENVIRONMENT "APS5_NO_IMAGE_SLABS=1")
    if(WIN32)
        set_tests_properties(agc_driver_image_memory_no_slabs agc_driver_image_memory_teardown_failure
                agc_driver_image_memory_no_slabs_teardown_failure PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
