# libSceNKWebKit

## add Functional

- [ISOWebVTTCue::toJSONString guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WebCoreIsoVttCue.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::ISOWebVTTCue::toJSONString` is unmeasured; the export copies the guest `WebVTTCue` bytes, `WTF::StringImpl` headers, and character buffers through `GuestReadable_nid_no_patch` / `GuestCopyIn_nid_no_patch` and calls `NotImplementedAt_nid_no_patch` on unmapped or unreadable guest memory. Closes when: console behavior for invalid guest pointers in `WebCore::ISOWebVTTCue::toJSONString` is verified on hardware.
