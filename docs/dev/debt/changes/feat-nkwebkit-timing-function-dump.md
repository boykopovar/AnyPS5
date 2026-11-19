# libSceNKWebKit

## remove `_ZN7WebCorelsERN3WTF10TextStreamERKNS_14TimingFunctionE`

## add Functional

- [WebCore TimingFunction dump](../../../core/libs/prx/libSceNKWebKit/src/TimingFunctionDump.cpp) (libSceNKWebKit) - not compared with a console module. `operator<<(TextStream&, const TimingFunction&)` reads the type through vtable slot 3 and writes `linear`, `cubic-bezier(x1, y1, x2, y2)`, `steps(n[, position])` or `spring(mass stiffness damping velocity)`, with the subclass field offsets of the pinned [TimingFunction.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/animation/TimingFunction.h) and the text of [TimingFunction.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/animation/TimingFunction.cpp#L41-L93). Closes when: compared with a console module.
