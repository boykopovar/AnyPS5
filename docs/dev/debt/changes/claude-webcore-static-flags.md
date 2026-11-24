# libSceNKWebKit

## add Unknown function info

- [WebCore static state flags](../../../core/libs/prx/libSceNKWebKit/src/PlatformKeyboardEvent.cpp) (libSceNKWebKit) - not compared with a console module. The [console exception](../../../core/libs/prx/libSceNKWebKit/src/PageConsoleClient.cpp) and [audio session](../../../core/libs/prx/libSceNKWebKit/src/PlatformMediaSessionManager.cpp) flags start false and keep the last value set. The modifier set stored by `setCurrentModifierState` is never read, since `getCurrentModifierState` is empty in the 11.00 module. The older `Process::setIdentifier` mangling forwards to the [ObjectIdentifierGeneric export](../../../core/libs/prx/libSceNKWebKit/src/WebCoreProcess.cpp).

## remove `_ZN7WebCore17PageConsoleClient21shouldPrintExceptionsEv`

## remove `_ZN7WebCore17PageConsoleClient24setShouldPrintExceptionsEb`

## remove `_ZN7WebCore27PlatformMediaSessionManager28shouldDeactivateAudioSessionEv`

## remove `_ZN7WebCore27PlatformMediaSessionManager31setShouldDeactivateAudioSessionEb`
