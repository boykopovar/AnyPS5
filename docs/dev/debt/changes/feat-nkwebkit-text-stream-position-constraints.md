# libSceNKWebKit

## add Functional

- [WebCore text dumps of position constraints](../../../core/libs/prx/libSceNKWebKit/src/ScrollingConstraintsDump.cpp) (libSceNKWebKit) - not compared with a console module. The `operator<<(TextStream&, ...)` overloads for `AbsolutePositionConstraints` and `FixedPositionViewportConstraints` follow [ScrollingConstraints.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/page/scrolling/ScrollingConstraints.cpp#L113-L126), the same at `e12a212a531a`, `73a35b43be7a` and in the pinned 11.x tree. The 16-byte and 48-byte layouts (viewport rect at offset 20, after the base's `unsigned`) come from [ScrollingConstraints.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/page/scrolling/ScrollingConstraints.h#L37-L129) laid out by the Itanium ABI. The `StickyPositionViewportConstraints` overload stays a stub: its text differs between those revisions and 11.x.

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE` | `3s8eZwwVACU` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE` | `gkrDbcAYMyA` |
