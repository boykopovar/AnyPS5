# libSceNKWebKit

## add Functional

- [Empty no-op exports](../../../core/libs/prx/libSceNKWebKit/src/NoOpExports.cpp) (libSceNKWebKit) - 150 exports whose body in the 11.00 `libSceNKWebKit.sprx` (SHA-256 3af992253665387feeb5df90ff27dff683e3aa270cc4d92a0535b46ab18fa20d`) is a single `ret` byte are empty `void` functions: `WKPageSetUserContentExtensionsEnabled`, the `JSDOMRectList`, `JSAudioTrack` and `JSAnimationTimeline` `finishCreation` hooks, the `FrameLoaderClient` and `EmptyFrameLoaderClient` notifications and the `LibWebRTCProvider` setters among them. The module writes no return value, so a caller reads whatever the previous instruction left in the register; declaring them `void` reproduces that instead of answering a constant. They join the 177 no-ops already there. Not measured on a console beyond their bodies.
