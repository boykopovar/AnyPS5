# libSceNKWebKit

## add Functional

- [WebCore::subdivide](../../../core/libs/prx/libSceNKWebKit/src/MarkedText.cpp) (libSceNKWebKit) - not compared with a console module. It follows [MarkedText.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/rendering/MarkedText.cpp) on 32-byte `MarkedText` records and returns the 16-byte `Vector` through a hidden pointer. WebKit orders equivalent entries with `std::sort`, whose tie order belongs to the module's C++ library (Dinkumware-derived, source not public). Inputs whose result depends on that order throw: two ranges of one type but different marker or highlight name that share a piece (or, for `Frontmost`, a start offset); an end at the smallest offset next to a begin, or next to an end of the same type; an unknown strategy. Some of these throw although every order agrees. Closes when: the module's `std::sort` is known.
