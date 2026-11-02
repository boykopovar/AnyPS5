add_test_executable(agc_driver_recorder_tests EXCLUDE_FROM_ALL
        tests/Recorder.cpp
        tests/HostImportLifetime.cpp
        tests/ResidentPresent.cpp
        ${agcTriangleHeaders}
        ${agcSampleLodShaderHeader}
        ${agcSampleArrayShaderHeader}
        ${agcCoverShaderHeaders}
        ${agcBufferUpdateHeader}
)

target_include_directories(agc_driver_recorder_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${agcTestShaderGeneratedRoot}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_recorder_tests PRIVATE
        libSceAgc
        libkernel
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_recorder_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_recorder_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_recorder_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_recorder_device_selection COMMAND agc_driver_recorder_tests --device-selection-only)
    add_test(NAME agc_gpu_timing_key COMMAND agc_driver_recorder_tests --gpu-timing-key-only)
    add_test(NAME agc_gpu_timing_complete COMMAND agc_driver_recorder_tests --gpu-timing-complete-only)
    add_test(NAME agc_gpu_timing_incomplete COMMAND agc_driver_recorder_tests --gpu-timing-incomplete-only)
    set_tests_properties(agc_gpu_timing_complete agc_gpu_timing_incomplete PROPERTIES
        ENVIRONMENT "APS5_PROFILE_GPU=1" TIMEOUT 15 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_recorder_device_selection agc_gpu_timing_key agc_gpu_timing_complete agc_gpu_timing_incomplete PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_command_memory_reuse COMMAND agc_driver_recorder_tests --command-reuse-only)
    set_tests_properties(agc_command_memory_reuse PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_command_memory_reuse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_pipeline_cache COMMAND agc_driver_recorder_tests --pipeline-cache-only)
    add_test(NAME agc_pipeline_cache_file COMMAND agc_driver_recorder_tests --pipeline-cache-file-only)
    add_test(NAME agc_pipeline_cache_header COMMAND agc_driver_recorder_tests --pipeline-cache-header-only)
    set_tests_properties(agc_pipeline_cache_header PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_pipeline_cache_header PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_pipeline_library COMMAND agc_driver_recorder_tests --pipeline-library-only)
    add_test(NAME agc_pipeline_library_off COMMAND agc_driver_recorder_tests --pipeline-library-only)
    set_tests_properties(agc_pipeline_library agc_pipeline_library_off PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(agc_pipeline_library_off PROPERTIES ENVIRONMENT "APS5_NO_GPL=1")
    add_test(NAME agc_pipeline_library_fast_only COMMAND agc_driver_recorder_tests --pipeline-library-only)
    set_tests_properties(agc_pipeline_library_fast_only PROPERTIES ENVIRONMENT "APS5_NO_PIPELINE_LTO=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_pipeline_library_lto_info COMMAND agc_driver_recorder_tests --pipeline-library-lto-info-only)
    add_test(NAME agc_pipeline_library_lto_info_fast_only COMMAND agc_driver_recorder_tests --pipeline-library-lto-info-only)
    set_tests_properties(agc_pipeline_library_lto_info agc_pipeline_library_lto_info_fast_only PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(agc_pipeline_library_lto_info_fast_only PROPERTIES ENVIRONMENT "APS5_NO_PIPELINE_LTO=1")
    if(WIN32)
        set_tests_properties(agc_pipeline_library agc_pipeline_library_off agc_pipeline_library_fast_only agc_pipeline_library_lto_info agc_pipeline_library_lto_info_fast_only PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_pipeline_cache agc_pipeline_cache_file PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_pipeline_cache agc_pipeline_cache_file PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_import_watch COMMAND agc_driver_recorder_tests --import-watch-only)
    set_tests_properties(agc_import_watch PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_import_settle_initial_dcc COMMAND agc_driver_recorder_tests --settle-initial-dcc-only)
    set_tests_properties(agc_import_settle_initial_dcc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        add_test(NAME agc_import_watch_other_family COMMAND agc_driver_recorder_tests --import-watch-other-family-only)
        set_tests_properties(agc_import_watch_other_family PROPERTIES SKIP_RETURN_CODE 77)
        add_test(NAME agc_import_settle_deferred COMMAND agc_driver_recorder_tests --deferred-settle-only)
        add_test(NAME agc_import_settle_deferred_other_family COMMAND agc_driver_recorder_tests --deferred-settle-other-family-only)
        add_test(NAME agc_import_settle_write_order COMMAND agc_driver_recorder_tests --settle-write-order-only)
        add_test(NAME agc_import_settle_write_order_other_family COMMAND agc_driver_recorder_tests --settle-write-order-other-family-only)
        add_test(NAME agc_import_settle_wait_failure COMMAND agc_driver_recorder_tests --settle-wait-failure-only)
        set_tests_properties(agc_import_settle_write_order agc_import_settle_write_order_other_family agc_import_settle_wait_failure PROPERTIES
            SKIP_RETURN_CODE 77 TIMEOUT 30
            FAIL_REGULAR_EXPRESSION "SYNC-HAZARD-|VUID-"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME agc_import_settle_first_use COMMAND agc_driver_recorder_tests --settle-first-use-only)
        add_test(NAME agc_import_settle_earlier_import COMMAND agc_driver_recorder_tests --settle-earlier-import-only)
        add_test(NAME agc_import_settle_retire COMMAND agc_driver_recorder_tests --settle-retire-only)
        add_test(NAME agc_import_settle_storage_image COMMAND agc_driver_recorder_tests --settle-storage-image-only)
        set_tests_properties(agc_import_settle_deferred agc_import_settle_deferred_other_family agc_import_settle_first_use agc_import_settle_earlier_import agc_import_settle_retire agc_import_settle_storage_image PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
        set_tests_properties(agc_import_watch agc_import_watch_other_family agc_import_settle_deferred agc_import_settle_deferred_other_family agc_import_settle_first_use agc_import_settle_earlier_import agc_import_settle_retire agc_import_settle_storage_image agc_import_settle_initial_dcc PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(WIN32)
        add_test(NAME agc_reimport_bench COMMAND agc_driver_recorder_tests --reimport-bench)
        add_test(NAME agc_host_import_cut_lifetime COMMAND agc_driver_recorder_tests --host-import-cut-only)
        set_tests_properties(agc_reimport_bench agc_host_import_cut_lifetime PROPERTIES SKIP_RETURN_CODE 77)
        set_tests_properties(agc_reimport_bench agc_host_import_cut_lifetime PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_unchanged_cpu_stamps COMMAND agc_driver_recorder_tests --unchanged-cpu-stamps-only)
    set_tests_properties(agc_unchanged_cpu_stamps PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_unchanged_cpu_stamps PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    foreach(mode IN ITEMS map ring shared uncached)
        add_test(NAME agc_collect_mapping_history_${mode} COMMAND agc_driver_recorder_tests --collect-mapping-history-only)
        set_tests_properties(agc_collect_mapping_history_${mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
        if(WIN32)
            set_tests_properties(agc_collect_mapping_history_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    set_tests_properties(agc_collect_mapping_history_ring PROPERTIES ENVIRONMENT "APS5_NO_EPOCH_COLLECT_MAP=1")
    set_tests_properties(agc_collect_mapping_history_shared PROPERTIES ENVIRONMENT "APS5_SHARED_COLLECT_MEMO=1")
    set_tests_properties(agc_collect_mapping_history_uncached PROPERTIES ENVIRONMENT "APS5_NO_COLLECT_MEMO=1")
    if(WIN32)
        foreach(mode IN ITEMS cached uncached)
            add_test(NAME agc_hint_mapping_history_${mode} COMMAND agc_driver_recorder_tests --hint-mapping-history-only)
            set_tests_properties(agc_hint_mapping_history_${mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endforeach()
        set_tests_properties(agc_hint_mapping_history_uncached PROPERTIES ENVIRONMENT "APS5_NO_COLLECT_MEMO=1")
        foreach(mode IN ITEMS cached uncached)
            add_test(NAME agc_hint_partial_block_store_${mode} COMMAND agc_driver_recorder_tests --hint-partial-block-store-only)
            set_tests_properties(agc_hint_partial_block_store_${mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endforeach()
        set_tests_properties(agc_hint_partial_block_store_uncached PROPERTIES ENVIRONMENT "APS5_NO_COLLECT_MEMO=1")
        foreach(mode IN ITEMS cached uncached)
            add_test(NAME agc_shared_block_rewatch_${mode} COMMAND agc_driver_recorder_tests --shared-block-rewatch-only)
            set_tests_properties(agc_shared_block_rewatch_${mode} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endforeach()
        set_tests_properties(agc_shared_block_rewatch_uncached PROPERTIES ENVIRONMENT "APS5_NO_COLLECT_MEMO=1")
        foreach(kind IN ITEMS complete possible)
            add_test(NAME agc_interior_watched_driver_write_${kind} COMMAND agc_driver_recorder_tests --interior-watched-driver-write-only ${kind})
            set_tests_properties(agc_interior_watched_driver_write_${kind} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endforeach()
    endif()
    if(WIN32)
        add_test(NAME agc_partial_watch_restoration COMMAND agc_driver_recorder_tests --partial-watch-restoration-only)
        set_tests_properties(agc_partial_watch_restoration PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_kept_unit_settle COMMAND agc_driver_recorder_tests --kept-unit-settle-only)
    add_test(NAME agc_storage_idle_writeback_race COMMAND agc_driver_recorder_tests --idle-writeback-race-only)
    set_tests_properties(agc_storage_idle_writeback_race PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    set_tests_properties(agc_storage_kept_unit_settle PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_storage_idle_writeback_race agc_storage_kept_unit_settle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_baseline_settled COMMAND agc_driver_recorder_tests --settled-baseline-only)
    add_test(NAME agc_storage_clear_hand_over COMMAND agc_driver_recorder_tests --clear-hand-over-only)
    set_tests_properties(agc_storage_clear_hand_over PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_storage_clear_hand_over PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_typed_fill_clear COMMAND agc_driver_recorder_tests --typed-fill-clear-only)
    set_tests_properties(agc_storage_typed_fill_clear PROPERTIES ENVIRONMENT "APS5_NO_PIPELINE_JOURNAL=1" SKIP_RETURN_CODE 77)
    add_test(NAME agc_storage_integer_clear COMMAND agc_driver_recorder_tests --integer-clear-only)
    set_tests_properties(agc_storage_integer_clear PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_storage_typed_fill_clear agc_storage_integer_clear PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_upload_lifetime COMMAND agc_driver_recorder_tests --storage-upload-lifetime-only)
    add_test(NAME agc_sampled_storage_base_level_usage COMMAND agc_driver_recorder_tests --sampled-storage-base-level-usage-only)
    set_tests_properties(agc_sampled_storage_base_level_usage PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_sampled_storage_base_level_usage PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_upload_failure COMMAND agc_driver_recorder_tests --storage-upload-failure-only)
    set_tests_properties(agc_storage_upload_lifetime agc_storage_upload_failure PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_storage_upload_lifetime agc_storage_upload_failure PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_storage_baseline_settled PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_storage_baseline_settled PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_bda_staging COMMAND agc_driver_recorder_tests --bda-staging-only)
    set_tests_properties(agc_bda_staging PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_bda_staging PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_read_written_staging COMMAND agc_driver_recorder_tests --read-written-staging-only)
    set_tests_properties(agc_read_written_staging PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_read_written_staging PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_written_buffer_order COMMAND agc_driver_recorder_tests --written-buffer-order-only)
    set_tests_properties(agc_storage_written_buffer_order PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_storage_written_buffer_order PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_split_writeback COMMAND agc_driver_recorder_tests --split-writeback-only)
    set_tests_properties(agc_storage_split_writeback PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    if(WIN32)
        set_tests_properties(agc_storage_split_writeback PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_write_back COMMAND agc_driver_recorder_tests --storage-write-back-only)
    set_tests_properties(agc_storage_write_back PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    if(WIN32)
        set_tests_properties(agc_storage_write_back PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_copied_direct_ordering COMMAND agc_driver_recorder_tests --copied-direct-only)
    set_tests_properties(agc_copied_direct_ordering PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_copied_direct_ordering PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_storage_writeback_blocks COMMAND agc_driver_recorder_tests --writeback-blocks-only)
    set_tests_properties(agc_storage_writeback_blocks PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    if(WIN32)
        set_tests_properties(agc_storage_writeback_blocks PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_recorder_read_list COMMAND agc_driver_recorder_tests --read-list-only)
    set_tests_properties(agc_recorder_read_list PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_deferred_failure_report COMMAND agc_driver_recorder_tests --deferred-failure-report-only)
    set_tests_properties(agc_recorder_deferred_failure_report PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_recorder_deferred_failure_report PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_space_pending_writes COMMAND agc_driver_recorder_tests --space-pending-writes-only)
    set_tests_properties(agc_space_pending_writes PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_space_pending_writes PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_host_import_unmap COMMAND agc_driver_recorder_tests --host-import-unmap-only)
    set_tests_properties(agc_host_import_unmap PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_parked_import_unmap COMMAND agc_driver_recorder_tests --parked-import-unmap-only)
    set_tests_properties(agc_parked_import_unmap PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    add_test(NAME agc_host_import_retirement COMMAND agc_driver_recorder_tests --host-import-retirement-only)
    set_tests_properties(agc_host_import_retirement PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_sparse_run_import COMMAND agc_driver_recorder_tests --sparse-run-import-only)
    set_tests_properties(agc_sparse_run_import PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_windowed_import_refresh COMMAND agc_driver_recorder_tests --windowed-import-refresh-only)
    set_tests_properties(agc_windowed_import_refresh PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_windowed_import_refresh PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_sparse_run_import_copied COMMAND agc_driver_recorder_tests --sparse-run-copied-only)
    set_tests_properties(agc_sparse_run_import_copied PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_unit_shadow COMMAND agc_driver_recorder_tests --unit-shadow-only)
    set_tests_properties(agc_unit_shadow PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000")
    add_test(NAME agc_idle_publish_wait COMMAND agc_driver_recorder_tests --idle-publish-wait-only)
    set_tests_properties(agc_idle_publish_wait PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT "APS5_PIN_WAIT_MS=3000;APS5_WRITE_WATCH_IMPORTS=watch")
    add_test(NAME agc_driver_bda_reads_shadowed_results COMMAND agc_driver_recorder_tests --bda-shadowed-results-only)
    add_test(NAME agc_driver_bda_reads_shadowed_results_per_build COMMAND agc_driver_recorder_tests --bda-shadowed-results-only)
    set_tests_properties(agc_driver_bda_reads_shadowed_results agc_driver_bda_reads_shadowed_results_per_build PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(agc_driver_bda_reads_shadowed_results_per_build PROPERTIES ENVIRONMENT "APS5_NO_ADDRESS_SPACE_CACHE=1")
    if(WIN32)
        set_tests_properties(agc_driver_bda_reads_shadowed_results agc_driver_bda_reads_shadowed_results_per_build PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_unit_shadow_completion_store COMMAND agc_driver_recorder_tests --completion-store-shadow-only)
    set_tests_properties(agc_unit_shadow_completion_store PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_unit_shadow_completion_store PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_recorder COMMAND agc_driver_recorder_tests)
    set_tests_properties(agc_driver_recorder PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        add_test(NAME agc_storage_cpu_snapshot COMMAND agc_driver_recorder_tests --storage-cpu-snapshot-only)
        set_tests_properties(agc_storage_cpu_snapshot PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" TIMEOUT 30 SKIP_RETURN_CODE 77
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_recorder_command_buffer_release COMMAND agc_driver_recorder_tests --command-buffer-release-only)
    set_tests_properties(agc_recorder_command_buffer_release PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_descriptor_pool_recycling COMMAND agc_driver_recorder_tests --descriptor-pool-only)
    set_tests_properties(agc_descriptor_pool_recycling PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_snapshot_cap COMMAND agc_driver_recorder_tests --draw-snapshot-cap-only)
    set_tests_properties(agc_draw_snapshot_cap PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_snapshot_epoch COMMAND agc_driver_recorder_tests --draw-snapshot-epoch-only)
    set_tests_properties(agc_draw_snapshot_epoch PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_multisample_guest_access COMMAND agc_driver_recorder_tests --multisample-only)
    set_tests_properties(agc_multisample_guest_access PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_repeat_storage_guard COMMAND agc_driver_recorder_tests --repeat-storage-guard-only)
    set_tests_properties(agc_repeat_storage_guard PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_inputs_in_place COMMAND agc_driver_recorder_tests --draw-inputs-in-place-only)
    add_test(NAME agc_pending_write_snapshot COMMAND agc_driver_recorder_tests --pending-write-snapshot-only)
    set_tests_properties(agc_pending_write_snapshot PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_pending_write_snapshot PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_tests_properties(agc_draw_inputs_in_place PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_draw_inputs_copied COMMAND agc_driver_recorder_tests --draw-inputs-copied-only)
    set_tests_properties(agc_draw_inputs_copied PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_resident_present COMMAND agc_driver_recorder_tests --resident-present-only)
    set_tests_properties(agc_resident_present PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_queued_write_groups COMMAND agc_driver_recorder_tests --queued-write-groups-only)
    add_test(NAME agc_tiled_resolve COMMAND agc_driver_recorder_tests --tiled-resolve-only)
    set_tests_properties(agc_tiled_resolve PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_slice_dirty COMMAND agc_driver_recorder_tests --slice-dirty-only)
    set_tests_properties(agc_slice_dirty PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME agc_metadata_pass COMMAND agc_driver_recorder_tests --metadata-pass-only)
    set_tests_properties(agc_metadata_pass PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(agc_recorder_read_list agc_host_import_unmap agc_parked_import_unmap agc_host_import_retirement agc_sparse_run_import agc_sparse_run_import_copied agc_unit_shadow agc_driver_recorder agc_idle_publish_wait agc_recorder_command_buffer_release agc_descriptor_pool_recycling agc_draw_snapshot_cap agc_draw_snapshot_epoch agc_multisample_guest_access agc_repeat_storage_guard agc_draw_inputs_in_place agc_draw_inputs_copied agc_resident_present agc_slice_dirty agc_queued_write_groups agc_metadata_pass agc_tiled_resolve PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME agc_host_import_second_mapping COMMAND agc_driver_recorder_tests --host-import-second-mapping-only)
        set_tests_properties(agc_host_import_second_mapping PROPERTIES SKIP_RETURN_CODE 77 ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_pending_key_stores COMMAND agc_driver_recorder_tests --pending-key-stores-only)
    set_tests_properties(agc_pending_key_stores PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_shadowed_key_stores COMMAND agc_driver_recorder_tests --shadowed-key-stores-only)
    set_tests_properties(agc_shadowed_key_stores PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_sample_dumps COMMAND agc_driver_recorder_tests --sample-dumps-only)
    set_tests_properties(agc_sample_dumps PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_uncounted_passes COMMAND agc_driver_recorder_tests --uncounted-passes-only)
    set_tests_properties(agc_uncounted_passes PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_draw_descriptors COMMAND agc_driver_recorder_tests --draw-descriptors-only)
    set_tests_properties(agc_draw_descriptors PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_pending_key_stores agc_shadowed_key_stores agc_sample_dumps agc_uncounted_passes agc_draw_descriptors PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_host_import_lifetime COMMAND agc_driver_recorder_tests --host-import-lifetime-only)
    add_test(NAME agc_driver_host_import_mutation_race COMMAND agc_driver_recorder_tests --host-import-mutation-race-only)
    set_tests_properties(agc_driver_host_import_lifetime agc_driver_host_import_mutation_race PROPERTIES
        ENVIRONMENT "APS5_PIN_WAIT_MS=2000" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_driver_unit_shadow COMMAND agc_driver_recorder_tests --unit-shadow-only)
    add_test(NAME agc_driver_unit_shadow_budget COMMAND agc_driver_recorder_tests --unit-shadow-only)
    set_tests_properties(agc_driver_unit_shadow PROPERTIES
        ENVIRONMENT "APS5_PIN_WAIT_MS=2000" SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(agc_driver_unit_shadow_budget PROPERTIES
        ENVIRONMENT "APS5_PIN_WAIT_MS=2000;APS5_UNIT_SHADOW_MIB=8" SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_small_tiled_shadow_label COMMAND agc_driver_recorder_tests --small-tiled-shadow-label-only)
    set_tests_properties(agc_small_tiled_shadow_label PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_completion_labels COMMAND agc_driver_recorder_tests --completion-labels-only)
    set_tests_properties(agc_recorder_completion_labels PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_cmask COMMAND agc_driver_recorder_tests --cmask-only)
    set_tests_properties(agc_recorder_cmask PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_cube COMMAND agc_driver_recorder_tests --cube-only)
    set_tests_properties(agc_recorder_cube PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME agc_recorder_failed_check COMMAND agc_driver_recorder_tests --failed-check-only)
    set_tests_properties(agc_recorder_failed_check PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30
        PASS_REGULAR_EXPRESSION "a failed check stopped the test with a texture upload in its open batch"
        FAIL_REGULAR_EXPRESSION "recorder teardown")
    if(WIN32)
        set_tests_properties(agc_driver_host_import_lifetime agc_driver_host_import_mutation_race agc_driver_unit_shadow agc_driver_unit_shadow_budget agc_small_tiled_shadow_label PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_tests_properties(agc_recorder_completion_labels agc_recorder_cmask agc_recorder_cube agc_recorder_failed_check PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    set_property(TEST agc_driver_host_import_lifetime APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "APS5_RELEASE_UNDER_LOCK=unset:" "APS5_RELEASE_ON_UNLOCK=unset:")
    if(WIN32)
        add_test(NAME agc_driver_host_import_lifetime_other_family COMMAND agc_driver_recorder_tests --host-import-lifetime-other-family-only)
        set_tests_properties(agc_driver_host_import_lifetime_other_family PROPERTIES
            ENVIRONMENT "APS5_PIN_WAIT_MS=2000" SKIP_RETURN_CODE 77 TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        set_property(TEST agc_driver_host_import_lifetime_other_family APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "APS5_RELEASE_UNDER_LOCK=unset:" "APS5_RELEASE_ON_UNLOCK=unset:")
    endif()
endif()

target_sources(agc_driver_recorder_tests PRIVATE ${agcVertexRangeHeaders})

if(BUILD_TESTING)
    add_test(NAME agc_driver_vertex_target_range_copied COMMAND agc_driver_recorder_tests --vertex-target-range-copied)
    add_test(NAME agc_driver_vertex_target_range_imported COMMAND agc_driver_recorder_tests --vertex-target-range-imported)
    set_tests_properties(agc_driver_vertex_target_range_copied agc_driver_vertex_target_range_imported PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_driver_vertex_target_range_copied agc_driver_vertex_target_range_imported PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
