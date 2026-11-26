# libSceIcu

## add Functional

- [`uprv_tzname_clear_cache`](../../../core/libs/prx/libSceIcu/LegacyExports.cpp) follows bundled ICU 78.1's `void()` ABI and forwards to its cache reset. This build disables ICU file I/O, so the Olson path cache is absent and the reset is a no-op on Linux and Windows; whether the console library keeps platform-specific time-zone state is unmeasured. Closes when: compared with the console.
