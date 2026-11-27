# libSceNKWebKit

## add Functional

- [GLContext::isExtensionSupported extensionList guest pointer fault behavior](../../../core/libs/prx/libSceNKWebKit/src/GLContextExtensions.cpp) (libSceNKWebKit) - Console fault behavior for invalid guest pointers in `WebCore::GLContext::isExtensionSupported` is unmeasured; the export copies `extensionList` in page-bounded chunks through `GuestCopyIn_nid_no_patch` and calls `NotImplementedAt_nid_no_patch` on unmapped, unreadable, or unterminated guest memory before inspecting or searching it. Closes when: console behavior for invalid guest pointers in `WebCore::GLContext::isExtensionSupported` is verified on hardware.
