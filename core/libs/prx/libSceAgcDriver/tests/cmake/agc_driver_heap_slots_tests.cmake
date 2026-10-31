add_test_executable(agc_driver_heap_slots_tests EXCLUDE_FROM_ALL
        tests/BindlessHeapSlots.cpp
)

target_include_directories(agc_driver_heap_slots_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_heap_slots_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_heap_slots_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_heap_slots_tests)

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_heap_slots_tests)
    add_test(NAME agc_driver_heap_slots_comes_back COMMAND agc_driver_heap_slots_tests comes-back)
    set_tests_properties(agc_driver_heap_slots_comes_back PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_heap_slots_comes_back PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(agcHeapSlotsMode IN ITEMS storage-comes-back storage-views-come-back mixed-comes-back views-come-back two-heaps-come-back owner-views-release shared-owner-release)
        string(REPLACE "-" "_" agcHeapSlotsName "${agcHeapSlotsMode}")
        add_test(NAME agc_driver_heap_slots_${agcHeapSlotsName} COMMAND agc_driver_heap_slots_tests ${agcHeapSlotsMode})
        set_tests_properties(agc_driver_heap_slots_${agcHeapSlotsName} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        if(WIN32)
            set_tests_properties(agc_driver_heap_slots_${agcHeapSlotsName} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()
