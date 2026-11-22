# libSceNKWebKit

## add Functional

- [EmptyFrameLoaderClient::dispatchDidReachLayoutMilestone](../../../core/libs/prx/libSceNKWebKit/src/EmptyClients.cpp) (libSceNKWebKit) - no-op in pinned WebKit 11.x [EmptyClients.cpp:798](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyClients.cpp#L798). The [header](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/EmptyFrameLoaderClient.h#L90) declares `OptionSet<LayoutMilestone>`, whose storage is 16-bit; console ABI is unverified.

## remove | `_ZN7WebCore22EmptyFrameLoaderClient31dispatchDidReachLayoutMilestoneEN3WTF9OptionSetINS_15LayoutMilestoneEEE` | `SwNTIE7lkj4` |
