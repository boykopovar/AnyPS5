# libSceNKWebKit

## add Functional

- [WKSecurityOriginCreate](../../../core/libs/prx/libSceNKWebKit/src/URL.cpp) (libSceNKWebKit) - follows the pinned WebKit guard `if (port && port <= 65535)`, so a negative port is converted to `uint16_t` (for example -1 becomes 65535); the console's handling of a negative port is unmeasured.
