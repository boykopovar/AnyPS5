# libSceNKWebKit

## add Unknown function info

- [RoundedRect::approximateAsRegion](../../../core/libs/prx/libSceNKWebKit/src/RoundedRectRegion.cpp) (libSceNKWebKit) - console ABI and behavior are unmeasured. Uses the pinned 48-byte `RoundedRect` and 24-byte `Region` layouts (`static_assert`) and follows [RoundedRect.cpp lines 314-383](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/RoundedRect.cpp#L314-L383) with 1/64 `LayoutUnit` arithmetic, uniting the enclosing integer rect and subtracting corner rectangles per arc step capped at 20. An unreadable input reference fails through `NotImplementedAt_nid_no_patch` and a zero `stepLength` on a non-empty rounded rect throws `std::invalid_argument` instead of faulting or dividing by zero. Closes when: measured on console.
