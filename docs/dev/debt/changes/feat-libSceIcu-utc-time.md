# libSceIcu

## add Functional

- [UTC time](../../../core/libs/prx/libSceIcu/UtcTime.cpp) - `uprv_getUTCtime` and `uprv_getRawUTCtime` follow the `HAVE_GETTIMEOFDAY` branch of ICU 68.2 [putil.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp#L300-L328), which `U_PLATFORM_IMPLEMENTS_POSIX` selects: guest `gettimeofday`, returned as whole milliseconds since the epoch. Unverified: that the console build takes that branch and not the `time()` one, which returns whole seconds, and that it is built without `U_DEBUG_FAKETIME`. ICU ignores a `gettimeofday` failure and reads the unset `timeval`; here a failure throws `NotImplemented_nid_no_patch`.
