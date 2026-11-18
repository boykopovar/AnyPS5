# TODO

- #23232: if asked, rerun the three failing agc_driver tests on the RTX 4070 SUPER with the Vulkan validation layer settings from AGENTS.md and post any `VUID-` lines. (Posting: person only.)
- test(tools): reproduce the one-off `debt_split` failure in `test_apply_changes_preserve_original_occurrences_and_upstream_entries` with `ctest -R ^debt_split -j 32 --repeat until-fail:50`; file an issue with the log if it fails. Not reproduced on 2026-11-18: 50 sequential `unittest` runs and the parallel ctest repeat both passed.
- build(windows): when a fix for #22358 merges, rebuild and confirm that `agc_driver_upload_cache_read_only_flush` builds and runs on Windows.
- #22427: run `agc_driver_stencil_plane_memory` on the RTX 4070 SUPER (driver 621.24) and add the result if that GPU is not listed yet. (Posting: person only.)
- #22930/#22931: run `agc_driver_msaa_dcc_unwatch` on the RTX 4070 SUPER (driver 621.24) and add the result if that GPU is not listed yet. (Posting: person only.)
- PR #22466: build it with WinLibs GCC 16.2.0 and comment whether the "multiple non-default manifests" link warning is gone. On `auto/hangar` the warning still appears with WinLibs GCC 16.2.0 r3 (relinker-only build, 2026-11-18). (Commenting: person only.)
- #22358: build the open fix PRs with WinLibs GCC 16.2.0 and comment which ones compile. (Commenting: person only.)
- After each upstream merge: run `py -3 -m unittest discover -s tools/tests` and the relinker ctest; file an issue for any Windows-only failure. Last run 2026-11-18 on `auto/hangar`: tools 3705 tests pass except `test_run_report_schema` (jsonschema not installed; `py -3 -m pip install -r tools/requirements-run-report.txt` is for the person), hw-oracle 131 pass, relinker ctest 1020/1020 pass.
- Full Windows build on `auto/hangar` (2026-11-19) fails only on `UploadCacheReadOnlyFlush.cpp:177` (`setenv`, #22358). Non-GPU ctest: 4253/4256 pass. Being fixed: `guest_libressl3_aes`, `guest_libressl3_aes_wrap`, `guest_audio_out_system_state_output_time_bounds`.
- Done 2026-11-18: docs links and debt links checked, none broken.
- Done 2026-11-18: docs(build) WinLibs GCC 16.2.0 r3 SEH (1648713f48).
- Skipped #23267 (reporter offered the test PR), #23262 (fix ready by reporter).
