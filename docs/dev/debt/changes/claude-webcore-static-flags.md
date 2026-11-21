# libSceNKWebKit

## add Unknown function info

- [WebCore static state flags](../../../core/libs/prx/libSceNKWebKit/src/PlatformKeyboardEvent.cpp) (libSceNKWebKit) - not compared with a console module. The [console exception](../../../core/libs/prx/libSceNKWebKit/src/PageConsoleClient.cpp) and [audio session](../../../core/libs/prx/libSceNKWebKit/src/PlatformMediaSessionManager.cpp) flags start false and keep the last value set. `getCurrentModifierState` always reports every modifier released, because the PlayStation [currentStateOfModifierKeys](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/libwpe/PlatformKeyboardEventLibWPE.cpp#L1349-L1352) returns the empty set; the modifier set stored by `setCurrentModifierState` is never read. The older `Process::setIdentifier` mangling forwards to the [ObjectIdentifierGeneric export](../../../core/libs/prx/libSceNKWebKit/src/WebCoreProcess.cpp).

## remove `_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv`

## remove `_ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb`

## remove `_ZN7WebCore21PlatformKeyboardEvent23getCurrentModifierStateERbS1_S1_S1_`

## remove `_ZN7WebCore27PlatformMediaSessionManager28shouldDeactivateAudioSessionEv`

## remove `_ZN7WebCore27PlatformMediaSessionManager31setShouldDeactivateAudioSessionEb`
