# libSceNKWebKit

## add Functional

- [JSC::MarkingConstraint::quickWorkEstimate](../../../core/libs/prx/libSceNKWebKit/src/MarkingConstraint.cpp) (libSceNKWebKit) - not compared with a console module. It returns 0 without reading the constraint or the visitor, as both the pinned [PS5 WebKit 11.x MarkingConstraint.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/MarkingConstraint.cpp#L70-L73) and WebKit [Safari-611.1.1](https://github.com/WebKit/WebKit/blob/Safari-611.1.1/Source/JavaScriptCore/heap/MarkingConstraint.cpp#L63-L66) do; the `double` result is returned in `xmm0`.

## remove | `_ZN3JSC17MarkingConstraint17quickWorkEstimateERNS_11SlotVisitorE` | `DD08+9TAImA` |
