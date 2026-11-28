# libSceNKWebKit

## add Functional

- [WebCore::SettingsBase guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/SettingsBase.cpp) - console behaviour on unreadable guest input ranges (including vtables, font-family hash tables, `WTF::StringImpl` storage, and `WTF::Vector` buffers) or unwritable guest fields (`m_minimumDOMTimerInterval`, `m_mediaContentTypesRequiringHardwareSupport`, and ref-counted `WTF::StringImpl` headers) passed to `SettingsBase` font-family getters/setters, `setMinimumDOMTimerInterval`, and `setMediaContentTypesRequiringHardwareSupport` is unmeasured; those paths throw through `NotImplementedAt_nid_no_patch` (or `std::invalid_argument` on null pointers) without mutating guest outputs. Closes when: console behaviour on invalid guest pointers in these exports is measured.
