# TODO

- #23232: if asked, rerun the three failing agc_driver tests on the RTX 4070 SUPER with the Vulkan validation layer settings from AGENTS.md and post any `VUID-` lines.
- docs(build): BUILD.md names WinLibs GCC 15.2.0; check the exception model of WinLibs 16.2.0 r3 (x86_64-ucrt-posix-seh), which builds the full tree here, and add it if it qualifies.
- test(tools): reproduce the one-off `debt_split` failure in `test_apply_changes_preserve_original_occurrences_and_upstream_entries` with `ctest -R ^debt_split -j 32 --repeat until-fail:50`; file an issue with the log if it fails.
- build(windows): when a fix for #22358 merges, rebuild and confirm that `agc_driver_upload_cache_read_only_flush` builds and runs on Windows.
- #22427: run `agc_driver_stencil_plane_memory` on the RTX 4070 SUPER (driver 621.24) and add the result if that GPU is not listed yet.
- #22930/#22931: run `agc_driver_msaa_dcc_unwatch` on the RTX 4070 SUPER (driver 621.24) and add the result if that GPU is not listed yet.
- PR #22466: build it with WinLibs GCC 16.2.0 and comment whether the "multiple non-default manifests" link warning is gone.
- #22358: build the open fix PRs with WinLibs GCC 16.2.0 and comment which ones compile.
- After each upstream merge: run `py -3 -m unittest discover -s tools/tests` and the relinker ctest; file an issue for any Windows-only failure.
- Skipped #23267 (reporter offered the test PR), #23262 (fix ready by reporter).
