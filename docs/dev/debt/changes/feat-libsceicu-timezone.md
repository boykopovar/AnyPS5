# libSceIcu

## add Functional

- [Timezone](../../../core/libs/prx/libSceIcu/Timezone.cpp) - `uprv_tzset`, `uprv_tzname` and `uprv_timezone` follow ICU 68.2 [putil.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp#L637-L680). These exports report the timezone the *host process* resolves: `uprv_timezone` reads the CRT `_timezone` on Windows and `uprv_tzname` falls back to the CRT `_tzname`. A guest `TZ` set through the guest libc is visible here on Windows because `tzset_nid_postfix` reaches the shared CRT, and it is not visible on POSIX, where the guest libc keeps its own zone state; the test therefore asserts the guest `TZ` path only under `_WIN32` and keeps platform-stable assertions elsewhere. Windows registry/`windowsZones` detection and POSIX `remapShortTimeZone` are not ported, Windows names are ANSI, and the console ABI is unmeasured.
