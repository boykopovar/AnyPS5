# libSceNKWebKit

## add Functional

- [WebCore::ImageBufferBackend::copyImagePixels](../../../core/libs/prx/libSceNKWebKit/src/ImageBufferBackend.cpp) (libSceNKWebKit) - not compared with a console module. Follows pinned [ImageBufferBackend.cpp@c55364d53f97](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/ImageBufferBackend.cpp#L259-L371) for all four `AlphaPremultiplication` transitions and `RGBA`/`BGRA` channel orderings, ignoring `this` as WebKit does. Unknown enum values, negative widths with positive heights, address overflow, and unreadable or unwritable guest pointers throw through `NotImplemented_nid_no_patch` instead of faulting; destination writability and source readability are checked before writing. Closes when: compared with a console module.
