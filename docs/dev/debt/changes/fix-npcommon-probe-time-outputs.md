# libSceNpCommon

## add Unknown function info

- [sce::np::Time date and timeout outputs](../../../core/libs/prx/libSceNpCommon/Runtime.cpp) - `Time::GetDateTime` writes the converted date with a checked guest write and returns `0x80558009` (invalid argument, as `sceNpRtcConvertToPosixTime` does) for an unwritable output; the three `Time::IsTimedoutSince*` methods write the optional remaining time the same way and, for an unwritable one, skip it and still return the timeout result. `errno` is preserved. The `Time` objects are still read in place. The console's handling of unmapped pointers was not measured.
