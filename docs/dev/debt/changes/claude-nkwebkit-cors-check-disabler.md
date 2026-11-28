# libSceNKWebKit

## add Unknown function info

- [WebCore::CrossOriginAccessControlCheckDisabler](../../../core/libs/prx/libSceNKWebKit/src/CrossOriginAccessControl.cpp) (libSceNKWebKit) - not compared with a console module. The vtable pointer at 0, the enabled flag at 8 and the 16-byte size follow the pinned [CrossOriginAccessControl.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.h#L79-L86) and the bodies follow [CrossOriginAccessControl.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/loader/CrossOriginAccessControl.cpp#L240-L257): the singleton starts enabled, copies copy the flag and assignment copies only the flag. The vtable's type-info slot is null, so guest RTTI on this class is unsupported. The deleting destructor frees through the guest global `operator delete`, since the class is not fast-allocated. Closes when: compared with a console module.

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisabler39setCrossOriginAccessControlCheckEnabledEb`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisabler9singletonEv`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1ERKS0_`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC1Ev`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC2ERKS0_`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerC2Ev`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisablerD0Ev`

## remove `_ZN7WebCore37CrossOriginAccessControlCheckDisableraSERKS0_`

## remove `_ZNK7WebCore37CrossOriginAccessControlCheckDisabler36crossOriginAccessControlCheckEnabledEv`
