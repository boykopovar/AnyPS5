target_link_libraries(agc_driver_guest_memory_lifetime_tests PRIVATE libkernel)

if(BUILD_TESTING)
    set(agcGuestMemoryLifetimeTests agc_driver_guest_memory_lifetime)
    foreach(mode IN ITEMS copy direct locked heap heap_uncached)
        set(testName agc_driver_guest_memory_lifetime_${mode})
        if(mode STREQUAL "heap_uncached")
            add_test(NAME ${testName} COMMAND agc_driver_guest_memory_lifetime_tests heap)
        else()
            add_test(NAME ${testName} COMMAND agc_driver_guest_memory_lifetime_tests ${mode})
        endif()
        list(APPEND agcGuestMemoryLifetimeTests ${testName})
    endforeach()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" AND Python3_Interpreter_FOUND AND ANYPS5_RELINKER_CLI)
        add_test(NAME agc_driver_guest_memory_lifetime_converted COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_guest_memory_lifetime.py
                ${CMAKE_SOURCE_DIR}/core/relinker/relinker/tests
                $<TARGET_FILE:relinker>
                $<TARGET_FILE:agc_guest_memory_lifetime_fixture>
                $<TARGET_FILE:libkernel>
                $<TARGET_FILE:libc>
                $<TARGET_FILE:libSceAgcDriver>)
        set_tests_properties(agc_driver_guest_memory_lifetime_converted PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIN_WAIT_MS=1000" SKIP_RETURN_CODE 77 TIMEOUT 30)
        add_asan_test_options(agc_driver_guest_memory_lifetime_converted verify_asan_link_order=0)
    endif()
endif()
