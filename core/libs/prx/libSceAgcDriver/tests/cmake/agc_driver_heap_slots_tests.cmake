add_test_executable(agc_driver_heap_slots_tests EXCLUDE_FROM_ALL
        tests/BindlessHeapSlots.cpp
)

target_include_directories(agc_driver_heap_slots_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
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
    foreach(agcHeapSlotsMode IN ITEMS comes-back storage-comes-back storage-views-come-back mixed-comes-back views-come-back two-heaps-come-back owner-views-release shared-owner-release)
        string(REPLACE "-" "_" agcHeapSlotsName "${agcHeapSlotsMode}")
        agc_add_env_test(agc_driver_heap_slots_${agcHeapSlotsName} agc_driver_heap_slots_tests
                ARGS ${agcHeapSlotsMode} ENV ANYPS5_NO_SHADER_CACHE=1)
    endforeach()
endif()
