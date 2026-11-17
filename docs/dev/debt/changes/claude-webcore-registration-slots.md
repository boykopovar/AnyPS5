# libSceNKWebKit

## add Unknown function info

- [WebCore registration slots](../../../core/libs/prx/libSceNKWebKit/src/WebCoreRegistrationSlots.cpp) (libSceNKWebKit) - not compared with a console module. The 8-byte `WebAnimation::instances` set and 4-byte `LocalLoadPolicy` are source-derived; the set stays empty as no `WebAnimation` constructor is implemented. `setSharedProvider` crashes off the main thread or when called twice, and its readers are not exported. Follows [WebAnimation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/animation/WebAnimation.cpp#L67-L71) and [MessagePortChannelProvider.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/messageports/MessagePortChannelProvider.cpp#L51-L56). Closes when: compared with a console module.

## remove `_ZN7WebCore12WebAnimation9instancesEv`

## remove `_ZN7WebCore14ResourceHandle20forceContentSniffingEv`

## remove `_ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE`

## remove `_ZN7WebCore18platformStrategiesEv`

## remove `_ZN7WebCore21setPlatformStrategiesEPNS_18PlatformStrategiesE`

## remove `_ZN7WebCore26MessagePortChannelProvider17setSharedProviderERS0_`
