if(BUILD_TESTING)
    add_test(NAME agc_driver_depth_plane_sampling_evicting COMMAND agc_driver_depth_plane_sampling_tests --evicting)
    set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT "APS5_DEPTH_PLANE_COPY_MIB=1" SKIP_RETURN_CODE 77 TIMEOUT 120)
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_sampling_evicting PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_epoch_exit COMMAND agc_driver_depth_plane_sampling_tests --epoch-exit)
    set_tests_properties(agc_driver_depth_plane_epoch_exit PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_epoch_exit PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_epoch_exit_no_fast COMMAND agc_driver_depth_plane_sampling_tests --epoch-exit-plane)
    set_tests_properties(agc_driver_depth_plane_epoch_exit_no_fast PROPERTIES ENVIRONMENT "APS5_NO_FAST_REVALIDATE=1" SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_epoch_exit_no_fast PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_epoch_exit_retired COMMAND agc_driver_depth_plane_sampling_tests --epoch-exit-retired)
    set_tests_properties(agc_driver_depth_plane_epoch_exit_retired PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_epoch_exit_retired PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_epoch_exit_verify COMMAND agc_driver_depth_plane_sampling_tests --epoch-exit)
    set_tests_properties(agc_driver_depth_plane_epoch_exit_verify PROPERTIES ENVIRONMENT "APS5_VERIFY_DRAW_PIPELINE=1" SKIP_RETURN_CODE 77 TIMEOUT 60
        PASS_REGULAR_EXPRESSION "3 reused resource proofs checked, 0 failed, 0 where the full proof stored to guest memory, 0 where only other threads broke proofs meanwhile, 0 where the full proof did work the exit skipped. 4 proved builds missed the exit, by input moved: [^\n]*surfaces 2, stale planes 2, per-use copies 0. moved alone: [^\n]*surfaces 1, stale planes 1, per-use copies 0\n.*depth plane epoch exit tests passed"
        FAIL_REGULAR_EXPRESSION "AGC graphics:|verify: the full proof behind a reused proof")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_epoch_exit_verify PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test(NAME agc_driver_depth_plane_epoch_exit_counts COMMAND agc_driver_depth_plane_sampling_tests --epoch-exit)
    set_tests_properties(agc_driver_depth_plane_epoch_exit_counts PROPERTIES ENVIRONMENT "APS5_PROFILE_EPOCH_PROOFS=1" SKIP_RETURN_CODE 77 TIMEOUT 60
        PASS_REGULAR_EXPRESSION "proved resource sets that missed the exit [4-9]. by input, moved alone/moved: [^\n]*surfaces 1/2, stale planes 1/2\n[^\n]*proof stamps by age when stored, stored/missed next/missed with the proof moved alone: current [0-9]+/[0-9]+/[0-9]+, own flushes 0/0/0, own walk 0/0/0, other threads 0/0/0, pending [0-9]+/[0-9]+/[0-9]+, other input 0/0/0\n[^\n]*proof breaks by site: [^\n]*write-backs empty 0, unchanged 0\n.*resource proofs reused 3,.*depth plane epoch exit tests passed"
        FAIL_REGULAR_EXPRESSION "AGC graphics:|verify: the full proof behind a reused proof")
    if(WIN32)
        set_tests_properties(agc_driver_depth_plane_epoch_exit_counts PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
