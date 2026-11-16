# libwebrtc

## change FastIntToBuffer

- [`absl::numbers_internal::FastIntToBuffer(int, char*)`](../../../core/libs/prx/libwebrtc/Export.cpp) now stages decimal text, its terminating NUL, and the existing eight-byte zero padding where applicable before publishing only the originally touched bytes through `GuestCopyOut_nid_no_patch`. A non-null inaccessible guest output throws `std::invalid_argument` instead of crashing the host; caller `errno` and all previously valid decimal and buffer-tail behavior are preserved. The native Abseil formatting algorithm and returned pointer offset remain unchanged; the retail-console response to an inaccessible non-null guest pointer has not been measured.
