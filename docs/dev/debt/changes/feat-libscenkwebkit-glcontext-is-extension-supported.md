# libSceNKWebKit

## add Unknown function info

- [GLContext extension lookup](../../../core/libs/prx/libSceNKWebKit/src/GLContextExtensions.cpp) has no console verification. It follows the existing graphics ports' PS5-WebKit revision, whose [`egl/GLContext.cpp` lines 566–580](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/egl/GLContext.cpp#L566-L580) has the matching static signature. Only the trailing space/NUL boundary is checked. A null list returns false even with a null extension; a null extension with a nonnull list throws instead of invalid access. An empty extension returns true for an empty or space-prefixed list; otherwise it throws because upstream's zero-length match and zero increment loop forever. Lengths above `INT_MAX` throw rather than narrow the upstream `int` length. Closes when: compared with a console module.
