# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE`

## add Functional

- [WebCore RectEdges<float> dump](../../../core/libs/prx/libSceNKWebKit/src/RectEdgesDump.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, const RectEdges<float>&)` writes `[top T right R bottom B left L]`, each side through the `TextStream` float writer, in the `std::array<float, 4>` order top, right, bottom, left of the pinned [RectEdges.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/RectEdges.h#L120-L125). Closes when: compared with a console module.
