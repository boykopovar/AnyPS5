# libSceNKWebKit

## add Functional

- [WTF Cairo reference helpers](../../../core/libs/prx/libSceNKWebKit/src/RefPtrCairo.cpp) (libSceNKWebKit) - not compared with a console module. The four `WTF::refIfNotNull` and `WTF::derefIfNotNull` specializations for `cairo_t` and `cairo_surface_t` follow pinned WebKit [RefPtrCairo.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/cairo/RefPtrCairo.cpp#L27-L49), ignoring null handles and forwarding non-null handles to `libcairo`'s `cairo_reference`, `cairo_destroy`, `cairo_surface_reference` and `cairo_surface_destroy`. Only Cairo handles created by `libcairo` are interoperable. Closes when: compared with a console module.
