# libSceNKWebKit

## add Unknown function info

- [FrameView viewport and fixed-position math](../../../core/libs/prx/libSceNKWebKit/src/WebCoreFrameView.cpp) (libSceNKWebKit) - not compared with a console module. The five static `FrameView` exports follow the pin's `LocalFrameView` ([lines 1664-1757](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/LocalFrameView.cpp#L1664-L1757), [1943-1996](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/LocalFrameView.cpp#L1943-L1996)) with 1/64 `LayoutUnit` arithmetic (`static_assert` layouts); negating `INT_MIN`, undefined upstream, wraps. Closes when: compared with a console module.
