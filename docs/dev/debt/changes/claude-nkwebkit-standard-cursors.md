# libSceNKWebKit

## add Functional

- [WebCore standard cursors](../../../core/libs/prx/libSceNKWebKit/src/Cursor.cpp) (libSceNKWebKit) - the 32-byte [Cursor layout](../../../core/libs/prx/libSceNKWebKit/src/CursorAbi.hpp) is derived from [Cursor.h L162-L174](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/Cursor.h#L162-L174) without `MOUSE_CURSOR_SCALE` and not compared with a console module; `Cursor::fromType` throws for values of 64 and up, whose console result is unknown. `handCursor`, `iBeamCursor` and `fromType` otherwise follow [Cursor.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/Cursor.cpp#L56-L275), with `Custom` and values up to 63 returning the pointer cursor. `pointerCursor` must return `StandardCursor(CursorType::Pointer)` when implemented. Closes when: compared with a console module.

## remove `_ZN7WebCore10handCursorEv`

## remove `_ZN7WebCore11iBeamCursorEv`

## remove `_ZN7WebCore6Cursor8fromTypeENS0_4TypeE`
