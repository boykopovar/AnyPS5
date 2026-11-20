# libSceNKWebKit

## add Functional

- [WebCore user-gesture and worker-count queries](../../../core/libs/prx/libSceNKWebKit/src/UserGestureIndicator.cpp) (libSceNKWebKit) - a non-null current `UserGestureToken` throws, because its layout (`RefCounted`, `CanMakeWeakPtr`, `m_state`, `m_scope`) is not modelled; only the unimplemented indicator constructors set it, so today the gesture and [typing](../../../core/libs/prx/libSceNKWebKit/src/UserTypingGestureIndicator.cpp) queries report no gesture and [workerThreadCount](../../../core/libs/prx/libSceNKWebKit/src/WorkerThread.cpp) reports 0. Behaviour follows Safari-610.1.28 [UserGestureIndicator.cpp](https://github.com/WebKit/WebKit/blob/Safari-610.1.28/Source/WebCore/dom/UserGestureIndicator.cpp#L39-L140) and [WorkerThread.cpp](https://github.com/WebKit/WebKit/blob/Safari-610.1.28/Source/WebCore/workers/WorkerThread.cpp#L66-L70), whose export names match the console; console runs are unverified. Closes when: the constructors and token layout exist and match a console module.

## remove `WKBundleIsProcessingUserGesture`

## remove `_ZN7WebCore12WorkerThread17workerThreadCountEv`

## remove `_ZN7WebCore16UserGestureToken60setMaximumIntervalForUserGestureForwardingForFetchForTestingEN3WTF7SecondsE`

## remove `_ZN7WebCore20UserGestureIndicator18currentUserGestureEv`

## remove `_ZN7WebCore20UserGestureIndicator29processingUserGestureForMediaEv`

## remove `_ZN7WebCore26UserTypingGestureIndicator27processingUserTypingGestureEv`

## remove `_ZN7WebCore26UserTypingGestureIndicator28focusedElementAtGestureStartEv`
