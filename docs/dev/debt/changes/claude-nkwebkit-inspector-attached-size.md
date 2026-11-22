# libSceNKWebKit

## add Unknown function info

- [InspectorFrontendClientLocal attached-window size constraints](../../../core/libs/prx/libSceNKWebKit/src/InspectorFrontendClientLocal.cpp) (libSceNKWebKit) - not compared with a console module. The static `constrainedAttachedWindowHeight` and `constrainedAttachedWindowWidth` follow [InspectorFrontendClientLocal.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/inspector/InspectorFrontendClientLocal.cpp#L414-L422): single-precision `roundf` of the preferred size clamped to at least 250 high and at most 0.75 of the window height, or at least 500 wide and at most the window width minus 320. The rounded value is converted with saturation, so a result at or above 2^32 returns 0xFFFFFFFF instead of an undefined conversion; with these inputs the width tops out at 4294967040. Closes when: compared with a console module.

## remove `_ZN7WebCore28InspectorFrontendClientLocal30constrainedAttachedWindowWidthEjj`

## remove `_ZN7WebCore28InspectorFrontendClientLocal31constrainedAttachedWindowHeightEjj`
