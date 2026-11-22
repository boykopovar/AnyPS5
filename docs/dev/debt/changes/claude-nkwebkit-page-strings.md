# libSceNKWebKit

## add Unknown function info

- [WebCore page strings](../../../core/libs/prx/libSceNKWebKit/src/WebCorePageStrings.cpp) (libSceNKWebKit) - unverified on a console; returns fresh 8-bit strings, not literal-backed ones. Follows [DisabledAdaptations.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/DisabledAdaptations.cpp#L33-L36) and [PerformanceLoggingClient.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/PerformanceLoggingClient.cpp#L31-L45). `standardUserAgent` and `standardUserAgentForURL` still throw: [UserAgentPlayStation.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/playstation/UserAgentPlayStation.cpp#L41-L59) reads name and version via unimplemented `getenv_np`; consoles report `PlayStation 5/<version>`. Closes when: compared with a console module.

## remove `_ZN7WebCore19watchAdaptationNameEv`

## remove `_ZN7WebCore24PerformanceLoggingClient35synchronousScrollingReasonsAsStringEN3WTF9OptionSetINS_26SynchronousScrollingReasonEEE`
