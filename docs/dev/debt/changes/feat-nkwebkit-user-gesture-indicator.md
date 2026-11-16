# libSceNKWebKit

## add Functional

- [User gesture indicators](../../../core/libs/prx/libSceNKWebKit/src/UserGestureIndicator.cpp) (libSceNKWebKit) - console behaviour unmeasured. The token-based constructors and destructors follow [UserGestureIndicator.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/dom/UserGestureIndicator.cpp#L82-L169) and share the `NKWebKit::UserGesture::CurrentToken` static with the gesture-state queries, which still throw while a token is installed. A weak-set bucket holding the last `WeakPtrImplWithEventTargetData` throws before any reference count is decremented, not `EventTargetData` teardown. A null `callableWrapper` observer throws `std::invalid_argument` where the console faults. The trailing `m_authorizationToken`, omitted by the console constructor, follows libc++ `std::optional` (payload first, flag second), unverified.

- [User gesture indicator constructors that throw](../../../core/libs/prx/libSceNKWebKit/src/UserGestureIndicator.cpp) (libSceNKWebKit) - the `WTF::Optional` constructor still throws: unmodeled `Document`, `ResourceLoadObserver` and `LocalDOMWindow`. `UserTypingGestureIndicator` still throws: the console symbol takes `Frame&`, the 11.x source `LocalFrame&`, and it needs unmodeled `LocalFrame::document`, `Document::focusedElement` and `Node`. Console behaviour is unmeasured.

## remove | `_ZN7WebCore20UserGestureIndicatorD1Ev` | `G-+SmxcuaMA` |

## remove | `_ZN7WebCore20UserGestureIndicatorD2Ev` | `y8w4lDQtlAo` |

## remove | `_ZN7WebCore16UserGestureTokenD1Ev` | `X62s7-3mQYs` |

## remove | `_ZN7WebCore16UserGestureTokenD2Ev` | `gLb38-yOLMw` |
