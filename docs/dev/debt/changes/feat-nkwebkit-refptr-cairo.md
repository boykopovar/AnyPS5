# libSceNKWebKit

## add Unknown function info

- [WTF::refIfNotNull and WTF::derefIfNotNull for cairo_t and cairo_surface_t](../../../core/libs/prx/libSceNKWebKit/src/RefPtrCairo.cpp) (libSceNKWebKit) - not compared with a console module. The four exports use the `template<> void refIfNotNull(T*)` form that WebKit replaced with `DefaultRefDerefTraits` before 11.x, so they follow [RefPtrCairo.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/cairo/RefPtrCairo.cpp#L27-L49) of August 2020: a null pointer is ignored, otherwise they call `cairo_reference`, `cairo_destroy`, `cairo_surface_reference` or `cairo_surface_destroy` in this project's libcairo. Whether the console module imports these from libcairo is unverified. Closes when: compared with a console module.
