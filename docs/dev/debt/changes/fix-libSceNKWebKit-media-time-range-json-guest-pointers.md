# libSceNKWebKit

## add Functional

- [MediaTimeRange::toJSONString guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WtfMediaTimeRangeJson.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WTF::MediaTimeRange::toJSONString` is unmeasured; the export copies the guest `MediaTimeRange` through `GuestCopyIn_nid_no_patch` and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Closes when: console behavior for invalid guest pointers in `WTF::MediaTimeRange::toJSONString` is verified on hardware.
