# libSceNKWebKit

## remove | `_ZN7WebCore14ScrollableArea32computeScrollbarValueAndOverhangEfffRfS1_` |

## add Unknown function info

- [Scrollbar value and overhang](../../../core/libs/prx/libSceNKWebKit/src/ScrollbarValue.cpp) - console ABI is unmeasured; the static member signature (no `this`) is assumed from [ScrollableArea.h:361](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/ScrollableArea.h#L361). Arithmetic and output write order follow [ScrollableArea.cpp:802–823](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/ScrollableArea.cpp#L802-L823), including aliased references and non-finite inputs. Closes when measured on console.
