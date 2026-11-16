# libwebrtc

## add Public source implementations

- [webrtc::RTCStatsReport begin, end and ConstIterator](../../../core/libs/prx/libwebrtc/src/RtcStatsReport.cpp) (libwebrtc) - console layout, reference counting and destructor dispatch remain unverified on hardware. The 40-byte `RTCStatsReport` (`RefCountedNonVirtual` atomic count at 0, `Timestamp` at 8, 24-byte `StatsMap` RB-tree of 80-byte nodes at 16) and 16-byte `ConstIterator` follow [rtc_stats_report.h](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/stats/rtc_stats_report.h) and [rtc_stats_report.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/stats/rtc_stats_report.cc), identical at M127 and M134, deleting `RTCStats` entries via virtual slot 1 and keys via `GuestStringStorage`. Invalid pointers, cyclic or inconsistent trees, or incrementing/dereferencing `end()` throw `std::invalid_argument` while preserving `errno`. Closes when: compared with the console.
