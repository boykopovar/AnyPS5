add_test_executable(agc_driver_pm4_tests EXCLUDE_FROM_ALL
        tests/Pm4.cpp
)

target_include_directories(agc_driver_pm4_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_pm4_tests PRIVATE
        libSceAgcDriver
        shader_recompiler_agc_driver
        libc
        libkernel
)

set_target_properties(agc_driver_pm4_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_pm4_tests)

add_test_executable(agc_driver_graphics_tests EXCLUDE_FROM_ALL
        tests/Graphics.cpp
        tests/ColorTargetLayout.cpp
        tests/BdaResources.cpp
        tests/GuestAllocations.cpp
        tests/LiveStack.cpp
        tests/TextureFormat.cpp
        tests/TextureTiling.cpp
        tests/GuestTextureResource.cpp
        tests/GuestSamplerResource.cpp
        tests/TextureDetiler.cpp
        tests/DepthSurfaceReuse.cpp
        tests/LastRecordIndices.cpp
)

target_include_directories(agc_driver_graphics_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        $<TARGET_PROPERTY:shader_recompiler_agc_driver,INTERFACE_INCLUDE_DIRECTORIES>
        ${agcTextureDetileGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/IntermediateRepresentation/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/ControlFlow/include
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler/RdnaDecoder/include
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_graphics_tests PRIVATE
        libSceAgcDriver
        libc
        libkernel
)

if(BUILD_TESTING)
    add_test(NAME agc_command_batch_fence_failure COMMAND agc_driver_graphics_tests --command-batch-fence-failure)
    set_tests_properties(agc_command_batch_fence_failure PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(agc_command_batch_fence_failure PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

set_target_properties(agc_driver_graphics_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_graphics_tests)

if(BUILD_TESTING)
    add_test(NAME agc_driver_undithered_alpha_to_coverage_state COMMAND agc_driver_graphics_tests --undithered-alpha-to-coverage-state)
    add_test(NAME agc_driver_depth_bounds_state COMMAND agc_driver_graphics_tests --depth-bounds-state)
    add_test(NAME agc_driver_msaa_dcc_state COMMAND agc_driver_graphics_tests --multisample-dcc-state)
    add_test(NAME agc_driver_unused_viewport_depth COMMAND agc_driver_graphics_tests --unused-viewport-depth)
    set_tests_properties(agc_driver_unused_viewport_depth PROPERTIES TIMEOUT 30)
    set_tests_properties(agc_driver_undithered_alpha_to_coverage_state agc_driver_depth_bounds_state PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_alpha_to_coverage_state COMMAND agc_driver_graphics_tests --alpha-to-coverage-state)
    set_tests_properties(agc_driver_alpha_to_coverage_state PROPERTIES
            ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_alpha_to_coverage_state agc_driver_undithered_alpha_to_coverage_state agc_driver_depth_bounds_state agc_driver_msaa_dcc_state agc_driver_unused_viewport_depth PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    agc_add_default_test(agc_driver_pm4_tests)
    agc_add_default_test(agc_driver_graphics_tests)
    add_test(NAME agc_sparse_buffer_holes COMMAND agc_driver_graphics_tests --sparse-buffer-holes)
    add_test(NAME agc_copy_input_kept_bytes COMMAND agc_driver_graphics_tests --copy-input-kept-bytes)
    add_test(NAME agc_depth_copy_kept_bytes COMMAND agc_driver_graphics_tests --depth-copy-kept-bytes)
    set_tests_properties(agc_copy_input_kept_bytes agc_depth_copy_kept_bytes PROPERTIES
            TIMEOUT 30 ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1"
            ENVIRONMENT_MODIFICATION "APS5_CPU_COPIES=unset:;APS5_GPU_COPY_MAX_KIB=unset:;APS5_CAPTURE_GPU_COPIES=unset:;APS5_BUFFER_POOL_SLOTS=unset:;APS5_BUFFER_POOL_SHARED=unset:;APS5_NO_BUFFER_CLASSES=unset:;APS5_STAGING_POOL_MIB=unset:")
    if(WIN32)
        set_property(TEST agc_copy_input_kept_bytes APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_property(TEST agc_depth_copy_kept_bytes APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_sparse_buffer_holes PROPERTIES TIMEOUT 30)
    add_test(NAME agc_pipeline_journal COMMAND agc_driver_graphics_tests --pipeline-journal)
    add_test(NAME agc_pipeline_journal_long_paths COMMAND agc_driver_graphics_tests --pipeline-journal-long-paths)
    set_tests_properties(agc_pipeline_journal_long_paths PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_pipeline_library_clear_waits_for_build COMMAND agc_driver_graphics_tests --pipeline-library-clear-waits-for-build)
    add_test(NAME agc_pipeline_store_lazy_buffer_pool COMMAND agc_driver_graphics_tests --pipeline-store-lazy-buffer-pool)
    add_test(NAME agc_pipeline_store_touch COMMAND agc_driver_graphics_tests --pipeline-store-touch)
    add_test(NAME agc_pipeline_store_growth COMMAND agc_driver_graphics_tests --pipeline-store-growth)
    add_test(NAME agc_pipeline_store_fixed_bound COMMAND agc_driver_graphics_tests --pipeline-store-growth)
    add_test(NAME agc_pipeline_store_replay_growth COMMAND agc_driver_graphics_tests --pipeline-store-replay-growth)
    set_tests_properties(agc_pipeline_store_growth agc_pipeline_store_fixed_bound agc_pipeline_store_replay_growth PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_pipeline_store_fixed_bound PROPERTIES ENVIRONMENT "APS5_PIPELINE_STORE_ENTRIES=1024")
    set_tests_properties(agc_pipeline_journal PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_pipeline_library_clear_waits_for_build PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_tests_properties(agc_pipeline_store_lazy_buffer_pool agc_pipeline_store_touch PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        add_test(NAME agc_pipeline_journal_extended_paths COMMAND agc_driver_graphics_tests --pipeline-journal-extended-paths)
        set_tests_properties(agc_pipeline_journal_extended_paths PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        set_tests_properties(agc_pipeline_journal_long_paths agc_pipeline_journal_extended_paths PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_pipeline_journal agc_pipeline_library_clear_waits_for_build agc_pipeline_store_lazy_buffer_pool agc_pipeline_store_touch agc_pipeline_store_growth agc_pipeline_store_fixed_bound agc_pipeline_store_replay_growth PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(WIN32)
        set_tests_properties(agc_sparse_buffer_holes PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_heap_mirror_unwatch COMMAND agc_driver_graphics_tests --heap-mirror-unwatch)
    add_test(NAME agc_heap_mirror_unwatch_no_space_cache COMMAND agc_driver_graphics_tests --heap-mirror-unwatch)
    add_test(NAME agc_sparse_heap_mirror_unwatch COMMAND agc_driver_graphics_tests --sparse-heap-mirror-unwatch)
    add_test(NAME agc_sparse_heap_mirror_unwatch_no_space_cache COMMAND agc_driver_graphics_tests --sparse-heap-mirror-unwatch)
    add_test(NAME agc_sparse_heap_mirror_budget COMMAND agc_driver_graphics_tests --sparse-heap-mirror-budget)
    add_test(NAME agc_heap_mirror_run_unwatch COMMAND agc_driver_graphics_tests --heap-mirror-run-unwatch)
    add_test(NAME agc_heap_mirror_run_unwatch_no_space_cache COMMAND agc_driver_graphics_tests --heap-mirror-run-unwatch)
    set_tests_properties(agc_heap_mirror_unwatch_no_space_cache agc_sparse_heap_mirror_unwatch_no_space_cache agc_heap_mirror_run_unwatch_no_space_cache PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_SPACE_CACHE=1")
    set_tests_properties(agc_heap_mirror_unwatch agc_heap_mirror_unwatch_no_space_cache agc_sparse_heap_mirror_unwatch agc_sparse_heap_mirror_unwatch_no_space_cache agc_heap_mirror_run_unwatch agc_heap_mirror_run_unwatch_no_space_cache agc_sparse_heap_mirror_budget PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_heap_mirror_unwatch agc_heap_mirror_unwatch_no_space_cache agc_sparse_heap_mirror_unwatch agc_sparse_heap_mirror_unwatch_no_space_cache agc_heap_mirror_run_unwatch agc_heap_mirror_run_unwatch_no_space_cache agc_sparse_heap_mirror_budget PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    else()
        add_test(NAME agc_heap_mirror_unwatched_import COMMAND agc_driver_graphics_tests --heap-mirror-unwatched-import)
        set_tests_properties(agc_heap_mirror_unwatched_import PROPERTIES TIMEOUT 30)
    endif()

    add_test(NAME agc_address_copy_cap COMMAND agc_driver_graphics_tests --address-copy-cap)
    add_test(NAME agc_address_copy_cap_remembered COMMAND agc_driver_graphics_tests --address-copy-cap-remembered)
    add_test(NAME agc_address_copy_cap_mirror COMMAND agc_driver_graphics_tests --address-copy-cap-mirror)
    add_test(NAME agc_address_space_unwatched_mirror_import COMMAND agc_driver_graphics_tests --unwatched-mirror-import)
    set_tests_properties(agc_address_copy_cap agc_address_copy_cap_remembered agc_address_copy_cap_mirror agc_address_space_unwatched_mirror_import PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_address_copy_cap agc_address_copy_cap_remembered agc_address_copy_cap_mirror agc_address_space_unwatched_mirror_import PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME agc_driver_graphics_maps_fallback COMMAND agc_driver_graphics_tests)
        set_tests_properties(agc_driver_graphics_maps_fallback PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_NO_PROCMAP_QUERY=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
    add_test(NAME agc_driver_pm4_failure COMMAND agc_driver_pm4_tests failure)
    set_tests_properties(agc_driver_pm4_failure PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set(pm4RingCases "ring|APS5_PM4_RING=4" "ring-empty|APS5_PM4_RING=16" "ring-disabled|APS5_PM4_RING=0" "ring-invalid|APS5_PM4_RING=4x" "ring-report|APS5_PM4_RING=2" "stall-watchdog|APS5_STALL_WATCHDOG_SECS=1"
            "stall-watchdog-invalid|APS5_STALL_WATCHDOG_SECS=1x" "stall-watchdog-rewind|APS5_STALL_WATCHDOG_SECS=1" "stall-watchdog-order|APS5_STALL_WATCHDOG_SECS=1")
    if(WIN32)
        list(APPEND pm4RingCases "ring-report-heap|APS5_PM4_RING=2")
    endif()
    foreach(pm4RingCase IN LISTS pm4RingCases)
        string(REPLACE "|" ";" pm4RingCase "${pm4RingCase}")
        list(GET pm4RingCase 0 pm4RingMode)
        list(GET pm4RingCase 1 pm4RingEnvironment)
        string(REPLACE "-" "_" pm4RingTest "agc_driver_pm4_${pm4RingMode}")
        add_test(NAME ${pm4RingTest} COMMAND agc_driver_pm4_tests ${pm4RingMode})
        set_tests_properties(${pm4RingTest} PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;${pm4RingEnvironment}" SKIP_RETURN_CODE 77 TIMEOUT 30)
        if(WIN32)
            set_tests_properties(${pm4RingTest} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME agc_descriptor_pool_reset_failure COMMAND agc_driver_graphics_tests --descriptor-pool-reset-failure)
    add_test(NAME agc_descriptor_cache COMMAND agc_driver_graphics_tests --descriptor-cache)
    add_test(NAME agc_driver_address_space_teardown COMMAND agc_driver_graphics_tests --address-space-teardown)
    set_tests_properties(agc_driver_address_space_teardown PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    add_test(NAME agc_driver_pm4_rewind_trace COMMAND agc_driver_pm4_tests rewind-trace)
    set_tests_properties(agc_driver_pm4_rewind_trace PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=8;APS5_TRACE_FRAMES=0:1;APS5_TRACE_DIR=${CMAKE_CURRENT_BINARY_DIR}/pm4-rewind-trace" TIMEOUT 30)
    set_property(TEST agc_driver_pm4_rewind_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_TRACE_SECONDS=unset:" "APS5_TRACE_SUMMARY_EVERY=unset:")
    if(WIN32)
        set_property(TEST agc_driver_pm4_rewind_trace APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_pm4_failure_inline COMMAND agc_driver_pm4_tests failure)
    set_tests_properties(agc_driver_pm4_failure_inline PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_PIPELINED_DRAWS=0" SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_driver_pm4_failure agc_driver_pm4_failure_inline agc_descriptor_pool_reset_failure agc_descriptor_cache agc_driver_address_space_teardown PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME agc_driver_pm4_priority_refused COMMAND agc_driver_pm4_tests priority-refused)
        set_tests_properties(agc_driver_pm4_priority_refused PROPERTIES ENVIRONMENT
            "ANYPS5_NO_SHADER_CACHE=1;APS5_THREAD_PRIORITY=high;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/aps5-no-rtkit" SKIP_RETURN_CODE 77)
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME agc_compute_descriptor_layout_flags COMMAND agc_driver_graphics_tests --descriptor-layout-flags)
    set_tests_properties(agc_compute_descriptor_layout_flags PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_covered_centroid_state COMMAND agc_driver_graphics_tests --covered-centroid-state)
    set_tests_properties(agc_driver_covered_centroid_state PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sample_location_grid_fallback COMMAND agc_driver_graphics_tests --sample-location-grid-fallback)
    set_tests_properties(agc_driver_sample_location_grid_fallback PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_unobserved_provoking_vertex COMMAND agc_driver_graphics_tests --unobserved-provoking-vertex)
    set_tests_properties(agc_driver_unobserved_provoking_vertex PROPERTIES TIMEOUT 30)
    add_test(NAME agc_driver_sample_order_planning COMMAND agc_driver_graphics_tests --sample-order)
    set_tests_properties(agc_driver_sample_order_planning PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_compute_descriptor_layout_flags PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_covered_centroid_state PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_sample_location_grid_fallback PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_unobserved_provoking_vertex PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_driver_sample_order_planning PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
