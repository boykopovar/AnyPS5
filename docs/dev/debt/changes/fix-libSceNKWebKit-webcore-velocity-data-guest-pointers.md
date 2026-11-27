# libSceNKWebKit

## add Functional

- [WebCoreVelocityData guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/WebCoreVelocityData.cpp) (libSceNKWebKit) - `WebCore::HistoricalVelocityData::velocityForNewData` and `WebCore::operator<<(WTF::TextStream&, const WebCore::VelocityData&)` validate guest `HistoricalVelocityData`, `TextStream`, and `VelocityData` pointers with checked guest reads/writes and call `NotImplementedAt_nid_no_patch` on unmapped or unreadable/unwritable guest memory. Console fault behavior is unmeasured. Closes when: console behavior for invalid guest pointers in `WebCore::HistoricalVelocityData::velocityForNewData` and `WebCore::operator<<(WTF::TextStream&, const WebCore::VelocityData&)` is verified on hardware.
