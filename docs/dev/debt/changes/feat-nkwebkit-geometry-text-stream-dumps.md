# libSceNKWebKit

## add Functional

- [WebCore::operator<<(TextStream&, const FloatRoundedRect&)](../../../core/libs/prx/libSceNKWebKit/src/FloatRoundedRectDump.cpp) (libSceNKWebKit) - not compared with a console module. It follows the pinned [FloatRoundedRect.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/FloatRoundedRect.cpp#L210-L218): the rectangle through this library's `FloatRect` operator, then one `dumpProperty` group per corner radius (`top-left`, `top-right`, `bottom-left`, `bottom-right`) through `startGroup`, the `FloatSize` operator and `endGroup`, on the 48-byte `FloatRoundedRect` of `WebCoreTypes.hpp`. Closes when: compared with a console module.
- [WebCore::operator<<(TextStream&, const RectEdges<float>&)](../../../core/libs/prx/libSceNKWebKit/src/FloatBoxExtentDump.cpp) (libSceNKWebKit) - not compared with a console module. The export is the non-template `FloatBoxExtent` overload of the pinned [LengthBox.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/LengthBox.cpp#L39-L43), not the bracketed template in `RectEdges.h`: `top: `, `right: `, `bottom: `, `left: ` with each side through `TextStream::operator<<(float)`, on a 16-byte `RectEdges<float>` (`std::array<float, 4>` in top, right, bottom, left order). Closes when: compared with a console module.

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE` | `0kJTcu2ueOE` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE` | `s8ZoG4mimcw` |
